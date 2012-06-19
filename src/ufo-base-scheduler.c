/**
 * SECTION:ufo-base-scheduler
 * @Short_description: Data transport between two UfoFilters
 * @Title: UfoBaseScheduler
 */

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include <string.h>

#include "config.h"
#include "ufo-base-scheduler.h"
#include "ufo-filter.h"
#include "ufo-filter-source.h"
#include "ufo-filter-sink.h"
#include "ufo-relation.h"

G_DEFINE_TYPE(UfoBaseScheduler, ufo_base_scheduler, G_TYPE_OBJECT)

#define UFO_BASE_SCHEDULER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BASE_SCHEDULER, UfoBaseSchedulerPrivate))

typedef struct {
    gfloat cpu_time;
    gfloat gpu_time;
} ExecutionInformation;

typedef struct {
    UfoFilter          *filter;
    GList              *relations;
    guint               num_cmd_queues;
    cl_command_queue   *cmd_queues;
    guint             **output_dims;
    GAsyncQueue       **input_pop_queues;
    GAsyncQueue       **input_push_queues;
    GAsyncQueue       **output_pop_queues;
    GAsyncQueue       **output_push_queues;
    UfoBuffer         **work;
    UfoBuffer         **result;
    GTimer            **timers;
} ThreadInfo;

struct _UfoBaseSchedulerPrivate {
    guint foo;
    GHashTable *exec_info;    /**< maps from gpointer to ExecutionInformation* */
};

/**
 * ufo_base_scheduler_new:
 *
 * Creates a new #UfoBaseScheduler.
 *
 * Return value: A new #UfoBaseScheduler
 */
UfoBaseScheduler *ufo_base_scheduler_new(void)
{
    UfoBaseScheduler *base_scheduler = UFO_BASE_SCHEDULER(g_object_new(UFO_TYPE_BASE_SCHEDULER, NULL));
    return base_scheduler;
}

static void
alloc_dim_sizes(GList *num_dims, guint **dims)
{
    guint i = 0;

    for (GList *it = g_list_first (num_dims); it != NULL; it = g_list_next (it), i++)
        dims[i] = g_malloc0 (((gsize) GPOINTER_TO_INT (it->data)) * sizeof(guint));
}

static void
push_poison_pill (GList *relations)
{
    g_list_foreach (relations, (GFunc) ufo_relation_push_poison_pill, NULL);
}

static void
alloc_output_buffers (UfoFilter *filter, GAsyncQueue *pop_queues[], guint **output_dims)
{
    UfoResourceManager *manager = ufo_resource_manager();
    GList *output_num_dims = ufo_filter_get_output_num_dims(filter);
    const guint num_outputs = ufo_filter_get_num_outputs(filter);

    for (guint port = 0; port < num_outputs; port++) {
        for (GList *it = g_list_first(output_num_dims); it != NULL; it = g_list_next(it)) {
            guint num_dims = (guint) GPOINTER_TO_INT(it->data);
            UfoBuffer *buffer = ufo_resource_manager_request_buffer (manager,
                    num_dims, output_dims[port], NULL, NULL);

            /*
             * For some reason, if we don't reference the buffer again,
             * it is returned by ufo_buffer_new() for other filters as
             * well, thus sharing the buffer which is not what we want.
             *
             * TODO: remove the reference in a meaningful way, or find
             * out what the problem is.
             */
            g_object_ref (buffer);

            g_async_queue_push(pop_queues[port], buffer);
        }
    }
}

static void
get_input_queues(GList *relations, UfoFilter *filter, GAsyncQueue ***input_push_queues, GAsyncQueue ***input_pop_queues)
{
    const guint num_inputs = ufo_filter_get_num_inputs(filter);
    GAsyncQueue **pop_queues = g_malloc0(sizeof(GAsyncQueue *) * num_inputs);
    GAsyncQueue **push_queues = g_malloc0(sizeof(GAsyncQueue *) * num_inputs);

    for (GList *it = g_list_first(relations); it != NULL; it = g_list_next(it)) {
        UfoRelation *relation = UFO_RELATION(it->data);

        if (ufo_relation_has_consumer(relation, filter)) {
            guint port = ufo_relation_get_consumer_port(relation, filter);
            ufo_relation_get_consumer_queues(relation, filter, &push_queues[port], &pop_queues[port]);
        }
    }

    *input_push_queues = push_queues;
    *input_pop_queues = pop_queues;
}

static void
get_output_queues(GList *relations, UfoFilter *filter, GAsyncQueue ***output_push_queues, GAsyncQueue ***output_pop_queues)
{
    const guint num_outputs = ufo_filter_get_num_outputs(filter);
    GAsyncQueue **pop_queues = g_malloc0(sizeof(GAsyncQueue *) * num_outputs);
    GAsyncQueue **push_queues = g_malloc0(sizeof(GAsyncQueue *) * num_outputs);

    for (GList *it = g_list_first(relations); it != NULL; it = g_list_next(it)) {
        UfoRelation *relation = UFO_RELATION(it->data);

        if (filter == ufo_relation_get_producer(relation)) {
            guint port = ufo_relation_get_producer_port(relation);
            ufo_relation_get_producer_queues(relation, &push_queues[port], &pop_queues[port]);
        }
    }

    *output_push_queues = push_queues;
    *output_pop_queues = pop_queues;
}

static gboolean
fetch_work (GAsyncQueue *pop_queues[], GAsyncQueue *push_queues[], UfoBuffer **work, guint num_inputs)
{
    const gpointer POISON_PILL = GINT_TO_POINTER(1);
    gboolean finish = FALSE;

    for (guint i = 0; i < num_inputs; i++) {
        work[i] = g_async_queue_pop (pop_queues[i]);

        if (work[i] == POISON_PILL) {
            g_async_queue_push (push_queues[i], POISON_PILL);
            finish = TRUE;
        }
    }

    return finish;
}

static GError *
process_source_filter (ThreadInfo *info)
{
    UfoFilter               *filter = UFO_FILTER (info->filter);
    UfoFilterSourceClass    *source_class = UFO_FILTER_SOURCE_GET_CLASS (filter);
    GError                  *error = NULL;
    const guint              num_outputs = ufo_filter_get_num_outputs(filter);
    gboolean                 cont = TRUE;
    const gchar             *name = ufo_filter_get_plugin_name (filter);

    error = source_class->source_initialize (UFO_FILTER_SOURCE (filter), info->output_dims);

    if (error != NULL)
        return error;

    alloc_output_buffers (filter, info->output_pop_queues, info->output_dims);

    while (cont) {
        g_print ("%s: pop output buffers\n", name);
        for (guint port = 0; port < num_outputs; port++)
            info->result[port] = g_async_queue_pop (info->output_pop_queues[port]);

        g_print ("%s: generate output\n", name);
        cont = source_class->generate (UFO_FILTER_SOURCE (filter), info->result, info->cmd_queues[0], &error);

        if (error != NULL)
            return error;

        if (cont) {
            g_print ("%s: push output\n", name);
            for (guint port = 0; port < num_outputs; port++)
                g_async_queue_push (info->output_push_queues[port], info->result[port]);
        }
    }

    return NULL;
}

static GError *
process_synchronous_filter (ThreadInfo *info)
{
    UfoFilter *filter = UFO_FILTER (info->filter);
    UfoFilterClass *filter_class = UFO_FILTER_GET_CLASS(filter);
    GError *error = NULL;
    gboolean finish = FALSE;
    const guint num_inputs = ufo_filter_get_num_inputs(filter);
    const guint num_outputs = ufo_filter_get_num_outputs(filter);
    const gchar             *name = ufo_filter_get_plugin_name (filter);

    /*
     * Initialize
     */
    if (!fetch_work (info->input_pop_queues, info->input_push_queues, info->work, num_inputs)) {
        ufo_filter_initialize (filter, info->work, info->output_dims, &error);

        if (error != NULL)
            return error;

        alloc_output_buffers (filter, info->output_pop_queues, info->output_dims);
    }

    while (!finish) {
        for (guint port = 0; port < num_outputs; port++)
            info->result[port] = g_async_queue_pop (info->output_pop_queues[port]);

        /*
         * Do the actual processing.
         */
        if (filter_class->process_gpu != NULL)
            error = filter_class->process_gpu(filter, info->work, info->result, info->cmd_queues[0]);
        else
            error = filter_class->process_cpu(filter, info->work, info->result, info->cmd_queues[0]);

        if (error != NULL)
            return error;

        /*
         * Release all work items so that preceding nodes can re-use
         * them.
         */
        for (guint port = 0; port < num_inputs; port++)
            g_async_queue_push(info->input_push_queues[port], info->work[port]);

        /*
         * Release all result items so that subsequent nodes can use them.
         */
        for (guint port = 0; port < num_outputs; port++)
            g_async_queue_push (info->output_push_queues[port], info->result[port]);

        finish = fetch_work (info->input_pop_queues, info->input_push_queues, info->work, num_inputs);
    }

    return NULL;
}

static GError *
process_sink_filter (ThreadInfo *info)
{
    UfoFilter *filter = UFO_FILTER (info->filter);
    UfoFilterSink *sink_filter = UFO_FILTER_SINK (filter);
    GError *error = NULL;
    gboolean finish = FALSE;
    const guint num_inputs = ufo_filter_get_num_inputs(filter);

    /*
     * Initialize
     */
    if (!fetch_work (info->input_pop_queues, info->input_push_queues, info->work, num_inputs)) {
        ufo_filter_sink_initialize (sink_filter, info->work, &error);

        if (error != NULL)
            return error;
    }

    while (!finish) {
        /*
         * Do the actual processing.
         */
        ufo_filter_sink_consume (sink_filter, info->work, info->cmd_queues[0], &error);

        if (error != NULL)
            return error;

        /*
         * Release all work items so that preceding nodes can re-use
         * them.
         */
        for (guint port = 0; port < num_inputs; port++)
            g_async_queue_push(info->input_push_queues[port], info->work[port]);

        finish = fetch_work (info->input_pop_queues, info->input_push_queues, info->work, num_inputs);
    }

    return NULL;
}

static gpointer
process_thread(gpointer data)
{
    GError *error = NULL;
    ThreadInfo *info = (ThreadInfo *) data;
    UfoFilter *filter = info->filter;

    g_object_ref (filter);

    const guint num_inputs = ufo_filter_get_num_inputs(filter);
    const guint num_outputs = ufo_filter_get_num_outputs(filter);

    GList *output_num_dims = ufo_filter_get_output_num_dims(filter);
    GList *producing_relations = NULL;
    info->input_pop_queues = NULL;
    info->input_push_queues = NULL;
    info->output_pop_queues = NULL;
    info->output_push_queues = NULL;
    info->work = g_malloc(num_inputs * sizeof(UfoBuffer *));
    info->result = g_malloc(num_outputs * sizeof(UfoBuffer *));
    info->timers = g_malloc(num_inputs * sizeof(GTimer *));
    info->output_dims = g_malloc0(num_outputs * sizeof(guint *));

    get_input_queues (info->relations, filter, &info->input_push_queues, &info->input_pop_queues);
    get_output_queues (info->relations, filter, &info->output_push_queues, &info->output_pop_queues);
    alloc_dim_sizes (output_num_dims, info->output_dims);

    /*
     * Find out, in which relations we are involved with this filter. This is
     * rather ugly and should be refactored at a later time.
     */
    for (GList *it = g_list_first(info->relations); it != NULL; it = g_list_next(it)) {
        UfoRelation *relation = UFO_RELATION(it->data);

        if (filter == ufo_relation_get_producer(relation))
            producing_relations = g_list_append(producing_relations, relation);
    }

    if (UFO_IS_FILTER_SOURCE (filter))
        error = process_source_filter (info);
    else if (UFO_IS_FILTER_SINK (filter))
        error = process_sink_filter (info);
    else
        error = process_synchronous_filter (info);

    push_poison_pill (producing_relations);

    g_free (info->output_dims);
    g_free (info->work);
    g_free (info->result);
    g_free (info->timers);
    g_free (info->input_pop_queues);
    g_free (info->input_push_queues);
    g_free (info->output_pop_queues);
    g_free (info->output_push_queues);
    g_list_free (producing_relations);
    g_object_unref (filter);

    return error;
}

/**
 * ufo_base_scheduler_run:
 * @scheduler: A #UfoBaseScheduler object
 * @error: return location for a GError with error codes from
 * #UfoPluginManagerError or %NULL
 *
 * Start executing all filters in their own threads.
 */
void ufo_base_scheduler_run(UfoBaseScheduler *scheduler, GList *relations, GError **error)
{
    g_return_if_fail(UFO_IS_BASE_SCHEDULER(scheduler));

    cl_command_queue *cmd_queues;
    guint num_queues;
    ufo_resource_manager_get_command_queues(ufo_resource_manager(), (void **) &cmd_queues, &num_queues);

    g_print ("what???\n");

    /*
     * Unfortunately, there is no set data type in GLib. Thus we have to write
     * this here.
     */
    GHashTable *filter_set = g_hash_table_new(g_direct_hash, g_direct_equal);

    for (GList *it = g_list_first(relations); it != NULL; it = g_list_next(it)) {
        UfoRelation *relation   = UFO_RELATION(it->data);
        UfoFilter   *producer   = ufo_relation_get_producer(relation);
        GList       *consumers  = ufo_relation_get_consumers(relation);

        g_hash_table_insert(filter_set, producer, NULL);

        for (GList *jt = g_list_first(consumers); jt != NULL; jt = g_list_next(jt))
            g_hash_table_insert(filter_set, jt->data, NULL);
    }

    GList *filters = g_hash_table_get_keys(filter_set);
    GList *threads = NULL;
    GError *tmp_error = NULL;
    ThreadInfo *thread_info = g_new0(ThreadInfo, g_list_length(filters));
    g_thread_init(NULL);

    GTimer *timer = g_timer_new();
    g_timer_start(timer);

    guint i = 0;

    /*
     * Start each filter in its own thread
     */
    for (GList *it = filters; it != NULL; it = g_list_next(it), i++) {
        UfoFilter *filter = UFO_FILTER(it->data);

        thread_info[i].filter = filter;
        thread_info[i].relations = relations;
        thread_info[i].num_cmd_queues = num_queues;
        thread_info[i].cmd_queues = cmd_queues;
        threads = g_list_append(threads, g_thread_create(process_thread, &thread_info[i], TRUE, &tmp_error));

        if (tmp_error != NULL) {
            /* FIXME: kill already started threads */
            g_propagate_error(error, tmp_error);
            return;
        }
    }

    /*
     * Wait for them to finish
     */
    for (GList *it = g_list_first(threads); it != NULL; it = g_list_next(it)) {
        tmp_error = (GError *) g_thread_join(it->data);
        if (tmp_error != NULL) {
            /* FIXME: wait for the rest? */
            g_propagate_error(error, tmp_error);
            return;
        }
    }

    g_free(thread_info);
    g_list_free(threads);
    g_list_free(filters);
    g_hash_table_destroy(filter_set);

    g_timer_stop(timer);
    g_message("Processing finished after %3.5f seconds", g_timer_elapsed(timer, NULL));
    g_timer_destroy(timer);
}

static void ufo_base_scheduler_dispose(GObject *object)
{
    UfoBaseSchedulerPrivate *priv = UFO_BASE_SCHEDULER_GET_PRIVATE(object);
    g_hash_table_destroy(priv->exec_info);
    G_OBJECT_CLASS(ufo_base_scheduler_parent_class)->dispose(object);
}

static void ufo_base_scheduler_finalize(GObject *object)
{
    G_OBJECT_CLASS(ufo_base_scheduler_parent_class)->finalize(object);
}

static void ufo_base_scheduler_class_init(UfoBaseSchedulerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = ufo_base_scheduler_dispose;
    gobject_class->finalize = ufo_base_scheduler_finalize;
    g_type_class_add_private(klass, sizeof(UfoBaseSchedulerPrivate));
}

static void ufo_base_scheduler_init(UfoBaseScheduler *base_scheduler)
{
    UfoBaseSchedulerPrivate *priv;
    base_scheduler->priv = priv = UFO_BASE_SCHEDULER_GET_PRIVATE(base_scheduler);
    priv->exec_info = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
}
