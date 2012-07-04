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

G_DEFINE_TYPE (UfoBaseScheduler, ufo_base_scheduler, G_TYPE_OBJECT)

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
    guint               num_inputs;
    guint               num_outputs;
    guint             **output_dims;
    GAsyncQueue       **input_pop_queues;
    GAsyncQueue       **input_push_queues;
    GAsyncQueue       **output_pop_queues;
    GAsyncQueue       **output_push_queues;
    UfoBuffer         **work;
    UfoBuffer         **result;
    GTimer            **timers;
    GTimer             *cpu_timer;
} ThreadInfo;

struct _UfoBaseSchedulerPrivate {
    GHashTable *exec_info;    /**< maps from gpointer to ExecutionInformation* */
};

/**
 * ufo_base_scheduler_new:
 *
 * Creates a new #UfoBaseScheduler.
 *
 * Return value: A new #UfoBaseScheduler
 */
UfoBaseScheduler *
ufo_base_scheduler_new (void)
{
    UfoBaseScheduler *base_scheduler = UFO_BASE_SCHEDULER (g_object_new (UFO_TYPE_BASE_SCHEDULER, NULL));
    return base_scheduler;
}

static void
alloc_dim_sizes (GList *num_dims, guint **dims)
{
    guint i = 0;

    for (GList *it = g_list_first (num_dims); it != NULL; it = g_list_next (it), i++)
        dims[i] = g_malloc0 (((gsize) GPOINTER_TO_INT (it->data)) * sizeof (guint));
}

static void
push_poison_pill (GList *relations)
{
    g_list_foreach (relations, (GFunc) ufo_relation_push_poison_pill, NULL);
}

static void
alloc_output_buffers (UfoFilter *filter, GAsyncQueue *pop_queues[], guint **output_dims)
{
    UfoResourceManager *manager = ufo_resource_manager ();
    GList *output_num_dims = ufo_filter_get_output_num_dims (filter);
    const guint num_outputs = ufo_filter_get_num_outputs (filter);

    for (guint port = 0; port < num_outputs; port++) {
        for (GList *it = g_list_first (output_num_dims); it != NULL; it = g_list_next (it)) {
            guint num_dims = (guint) GPOINTER_TO_INT (it->data);
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

            g_async_queue_push (pop_queues[port], buffer);
        }
    }
}

static void
get_input_queues (GList *relations, UfoFilter *filter, GAsyncQueue ***input_push_queues, GAsyncQueue ***input_pop_queues)
{
    const guint num_inputs = ufo_filter_get_num_inputs (filter);
    GAsyncQueue **pop_queues = g_malloc0 (sizeof(GAsyncQueue *) * num_inputs);
    GAsyncQueue **push_queues = g_malloc0 (sizeof(GAsyncQueue *) * num_inputs);

    for (GList *it = g_list_first (relations); it != NULL; it = g_list_next (it)) {
        UfoRelation *relation = UFO_RELATION(it->data);

        if (ufo_relation_has_consumer (relation, filter)) {
            guint port = ufo_relation_get_consumer_port (relation, filter);
            ufo_relation_get_consumer_queues (relation, filter, &push_queues[port], &pop_queues[port]);
        }
    }

    *input_push_queues = push_queues;
    *input_pop_queues = pop_queues;
}

static void
get_output_queues (GList *relations, UfoFilter *filter, GAsyncQueue ***output_push_queues, GAsyncQueue ***output_pop_queues)
{
    const guint num_outputs = ufo_filter_get_num_outputs (filter);
    GAsyncQueue **pop_queues = g_malloc0 (sizeof (GAsyncQueue *) * num_outputs);
    GAsyncQueue **push_queues = g_malloc0 (sizeof (GAsyncQueue *) * num_outputs);

    for (GList *it = g_list_first (relations); it != NULL; it = g_list_next (it)) {
        UfoRelation *relation = UFO_RELATION (it->data);

        if (filter == ufo_relation_get_producer (relation)) {
            guint port = ufo_relation_get_producer_port (relation);
            ufo_relation_get_producer_queues (relation, &push_queues[port], &pop_queues[port]);
        }
    }

    *output_push_queues = push_queues;
    *output_pop_queues = pop_queues;
}

static gboolean
fetch_work (ThreadInfo *info)
{
    const gpointer POISON_PILL = GINT_TO_POINTER (1);
    gboolean success = TRUE;

    for (guint i = 0; i < info->num_inputs; i++) {
        info->work[i] = g_async_queue_pop (info->input_pop_queues[i]);

        if (info->work[i] == POISON_PILL) {
            g_async_queue_push (info->input_push_queues[i], POISON_PILL);
            success = FALSE;
        }
    }

    return success;
}

static void
push_work (ThreadInfo *info)
{
    for (guint port = 0; port < info->num_inputs; port++)
        g_async_queue_push (info->input_push_queues[port], info->work[port]);
}

static void
fetch_result (ThreadInfo *info)
{
    for (guint port = 0; port < info->num_outputs; port++)
        info->result[port] = g_async_queue_pop (info->output_pop_queues[port]);
}

static void
push_result (ThreadInfo *info)
{
    for (guint port = 0; port < info->num_outputs; port++)
        g_async_queue_push (info->output_push_queues[port], info->result[port]);
}

static GError *
process_source_filter (ThreadInfo *info)
{
    UfoFilter               *filter = UFO_FILTER (info->filter);
    UfoFilterSourceClass    *source_class = UFO_FILTER_SOURCE_GET_CLASS (filter);
    GError                  *error = NULL;
    gboolean                 cont = TRUE;

    source_class->source_initialize (UFO_FILTER_SOURCE (filter), info->output_dims, &error);

    if (error != NULL)
        return error;

    alloc_output_buffers (filter, info->output_pop_queues, info->output_dims);

    while (cont) {
        fetch_result (info);

        g_timer_continue (info->cpu_timer);
        cont = source_class->generate (UFO_FILTER_SOURCE (filter), info->result, info->cmd_queues[0], &error);
        g_timer_stop (info->cpu_timer);

        if (error != NULL)
            return error;

        if (cont)
            push_result (info);
    }

    return NULL;
}

static GError *
process_synchronous_filter (ThreadInfo *info)
{
    UfoFilter *filter = UFO_FILTER (info->filter);
    UfoFilterClass *filter_class = UFO_FILTER_GET_CLASS (filter);
    GError *error = NULL;
    gboolean cont = TRUE;

    /*
     * Initialize
     */
    if (fetch_work (info)) {
        ufo_filter_initialize (filter, info->work, info->output_dims, &error);

        if (error != NULL)
            return error;

        alloc_output_buffers (filter, info->output_pop_queues, info->output_dims);
    }
    else {
        return NULL;
    }

    while (cont) {
        fetch_result (info);

        if (filter_class->process_gpu != NULL) {
            GList *events;

            events = filter_class->process_gpu (filter, info->work, info->result, info->cmd_queues[0], &error);
            g_list_free (events);
        }
        else {
            g_timer_continue (info->cpu_timer);
            filter_class->process_cpu (filter, info->work, info->result, info->cmd_queues[0], &error);
            g_timer_stop (info->cpu_timer);
        }

        if (error != NULL)
            return error;

        push_work (info);
        push_result (info);

        cont = fetch_work (info);
    }

    return NULL;
}

static GError *
process_sink_filter (ThreadInfo *info)
{
    UfoFilter *filter = UFO_FILTER (info->filter);
    UfoFilterSink *sink_filter = UFO_FILTER_SINK (filter);
    GError *error = NULL;
    gboolean cont = TRUE;
    const guint num_inputs = ufo_filter_get_num_inputs (filter);

    if (fetch_work (info)) {
        ufo_filter_sink_initialize (sink_filter, info->work, &error);

        if (error != NULL)
            return error;
    }
    else
        return NULL;

    while (cont) {
        g_timer_continue (info->cpu_timer);
        ufo_filter_sink_consume (sink_filter, info->work, info->cmd_queues[0], &error);
        g_timer_stop (info->cpu_timer);

        if (error != NULL)
            return error;

        for (guint port = 0; port < num_inputs; port++)
            g_async_queue_push (info->input_push_queues[port], info->work[port]);

        cont = fetch_work (info);
    }

    return NULL;
}

static void
alloc_info_data (ThreadInfo *info)
{
    info->work = g_malloc (info->num_inputs * sizeof (UfoBuffer *));
    info->result = g_malloc (info->num_outputs * sizeof (UfoBuffer *));
    info->timers = g_malloc (info->num_inputs * sizeof (GTimer *));
    info->output_dims = g_malloc0(info->num_outputs * sizeof (guint *));
}

static gpointer
process_thread (gpointer data)
{
    GError *error = NULL;
    ThreadInfo *info = (ThreadInfo *) data;
    UfoFilter *filter = info->filter;

    g_object_ref (filter);

    GList *output_num_dims = ufo_filter_get_output_num_dims (filter);
    GList *producing_relations = NULL;

    info->num_inputs = ufo_filter_get_num_inputs (filter);
    info->num_outputs = ufo_filter_get_num_outputs (filter);

    get_input_queues (info->relations, filter, &info->input_push_queues, &info->input_pop_queues);
    get_output_queues (info->relations, filter, &info->output_push_queues, &info->output_pop_queues);

    alloc_info_data (info);
    alloc_dim_sizes (output_num_dims, info->output_dims);

    /*
     * Find out, in which relations we are involved with this filter. This is
     * rather ugly and should be refactored at a later time.
     */
    for (GList *it = g_list_first (info->relations); it != NULL; it = g_list_next (it)) {
        UfoRelation *relation = UFO_RELATION (it->data);

        if (filter == ufo_relation_get_producer (relation))
            producing_relations = g_list_append (producing_relations, relation);
    }

    if (UFO_IS_FILTER_SOURCE (filter))
        error = process_source_filter (info);
    else if (UFO_IS_FILTER_SINK (filter))
        error = process_sink_filter (info);
    else
        error = process_synchronous_filter (info);

    push_poison_pill (producing_relations);

    g_print ("%s took %3.5f CPU seconds\n", ufo_filter_get_plugin_name (filter),
            g_timer_elapsed (info->cpu_timer, NULL));

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
void ufo_base_scheduler_run (UfoBaseScheduler *scheduler, GList *relations, GError **error)
{
    g_return_if_fail (UFO_IS_BASE_SCHEDULER (scheduler));

    cl_command_queue *cmd_queues;
    guint num_queues;
    ufo_resource_manager_get_command_queues (ufo_resource_manager (), (void **) &cmd_queues, &num_queues);

    /*
     * Unfortunately, there is no set data type in GLib. Thus we have to write
     * this here.
     */
    GHashTable *filter_set = g_hash_table_new (g_direct_hash, g_direct_equal);

    for (GList *it = g_list_first (relations); it != NULL; it = g_list_next (it)) {
        UfoRelation *relation   = UFO_RELATION (it->data);
        UfoFilter   *producer   = ufo_relation_get_producer (relation);
        GList       *consumers  = ufo_relation_get_consumers (relation);

        g_hash_table_insert (filter_set, producer, NULL);

        for (GList *jt = g_list_first (consumers); jt != NULL; jt = g_list_next (jt))
            g_hash_table_insert (filter_set, jt->data, NULL);
    }

    GList *filters = g_hash_table_get_keys (filter_set);
    GList *threads = NULL;
    GError *tmp_error = NULL;
    ThreadInfo *thread_info = g_new0(ThreadInfo, g_list_length (filters));
    g_thread_init (NULL);

    GTimer *timer = g_timer_new ();
    g_timer_start (timer);

    guint i = 0;

    /*
     * Start each filter in its own thread
     */
    for (GList *it = filters; it != NULL; it = g_list_next (it), i++) {
        UfoFilter *filter = UFO_FILTER (it->data);

        thread_info[i].filter = filter;
        thread_info[i].relations = relations;
        thread_info[i].num_cmd_queues = num_queues;
        thread_info[i].cmd_queues = cmd_queues;
        thread_info[i].cpu_timer = g_timer_new ();
        g_timer_stop (thread_info[i].cpu_timer);

        threads = g_list_append (threads, g_thread_create (process_thread, &thread_info[i], TRUE, &tmp_error));

        if (tmp_error != NULL) {
            /* FIXME: kill already started threads */
            g_propagate_error (error, tmp_error);
            return;
        }
    }

    /*
     * Wait for them to finish
     */
    for (GList *it = g_list_first (threads); it != NULL; it = g_list_next (it)) {
        tmp_error = (GError *) g_thread_join (it->data);
        if (tmp_error != NULL) {
            /* FIXME: wait for the rest? */
            g_propagate_error (error, tmp_error);
            return;
        }
    }

    /* TODO: free the cpu timers */

    g_free (thread_info);
    g_list_free (threads);
    g_list_free (filters);
    g_hash_table_destroy (filter_set);

    g_timer_stop (timer);
    g_message ("Processing finished after %3.5f seconds", g_timer_elapsed (timer, NULL));
    g_timer_destroy (timer);
}

static void ufo_base_scheduler_dispose (GObject *object)
{
    UfoBaseSchedulerPrivate *priv = UFO_BASE_SCHEDULER_GET_PRIVATE (object);
    g_hash_table_destroy (priv->exec_info);
    G_OBJECT_CLASS (ufo_base_scheduler_parent_class)->dispose (object);
}

static void ufo_base_scheduler_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_base_scheduler_parent_class)->finalize (object);
}

static void ufo_base_scheduler_class_init (UfoBaseSchedulerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->dispose = ufo_base_scheduler_dispose;
    gobject_class->finalize = ufo_base_scheduler_finalize;
    g_type_class_add_private (klass, sizeof (UfoBaseSchedulerPrivate));
}

static void ufo_base_scheduler_init (UfoBaseScheduler *base_scheduler)
{
    UfoBaseSchedulerPrivate *priv;
    base_scheduler->priv = priv = UFO_BASE_SCHEDULER_GET_PRIVATE (base_scheduler);
    priv->exec_info = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
}
