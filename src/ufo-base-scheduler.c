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

static void alloc_dim_sizes(GList *num_dims, guint **dims)
{
    guint i = 0;

    for (GList *it = g_list_first(num_dims); it != NULL; it = g_list_next(it), i++)
        dims[i] = g_malloc0(((gsize) GPOINTER_TO_INT(it->data)) * sizeof(guint));
}

static void push_poison_pill(GList *relations)
{
    g_list_foreach(relations, (GFunc) ufo_relation_push_poison_pill, NULL);
}

static gpointer process_thread(gpointer data)
{
    GError *error = NULL;
    ThreadInfo *info = (ThreadInfo *) data;
    UfoResourceManager *manager = ufo_resource_manager();
    UfoFilter *filter = info->filter;
    UfoFilterClass *filter_class = UFO_FILTER_GET_CLASS(filter);

    if ((filter_class->process_cpu == NULL) && (filter_class->process_gpu == NULL)) {
        g_warning("Filter is deprecated\n");
        return NULL;
    }

    g_object_ref (filter);

    const guint num_inputs = ufo_filter_get_num_inputs(filter);
    const guint num_outputs = ufo_filter_get_num_outputs(filter);
    const gpointer POISON_PILL = GINT_TO_POINTER(1);
    enum { INIT, WORK, FINISH } state = INIT;

    GList *input_num_dims = ufo_filter_get_input_num_dims(filter);
    GList *output_num_dims = ufo_filter_get_output_num_dims(filter);
    GList *producing_relations = NULL;
    GAsyncQueue **input_pop_queues = g_malloc0(sizeof(GAsyncQueue *) * num_inputs); 
    GAsyncQueue **input_push_queues = g_malloc0(sizeof(GAsyncQueue *) * num_inputs); 
    GAsyncQueue **output_pop_queues = g_malloc0(sizeof(GAsyncQueue *) * num_outputs); 
    GAsyncQueue **output_push_queues = g_malloc0(sizeof(GAsyncQueue *) * num_outputs); 
    UfoBuffer **work = g_malloc(num_inputs * sizeof(UfoBuffer *));
    UfoBuffer **result = g_malloc(num_outputs * sizeof(UfoBuffer *));
    GTimer **timers = g_malloc(num_inputs * sizeof(GTimer *));
    guint **input_dims = g_malloc0(num_inputs * sizeof(guint *));
    guint **output_dims = g_malloc0(num_outputs * sizeof(guint *));

    /*
     * Find out, in which relations we are involved with this filter. This is
     * rather ugly and should be refactored at a later time.
     */
    for (GList *it = g_list_first(info->relations); it != NULL; it = g_list_next(it)) {
        UfoRelation *relation = UFO_RELATION(it->data);

        if (filter == ufo_relation_get_producer(relation)) {
            guint port = ufo_relation_get_producer_port(relation);
            ufo_relation_get_producer_queues(relation, &output_push_queues[port], &output_pop_queues[port]);
            producing_relations = g_list_append(producing_relations, relation);
        }
        else if (ufo_relation_has_consumer(relation, filter)) {
            guint port = ufo_relation_get_consumer_port(relation, filter);
            ufo_relation_get_consumer_queues(relation, filter, &input_push_queues[port], &input_pop_queues[port]);
        }
    }

    alloc_dim_sizes(input_num_dims, input_dims);
    alloc_dim_sizes(output_num_dims, output_dims);

    while (state != FINISH) {
        /* 
         * Collect work buffers from all input port queues. In case there is no
         * more work, we will pass on this information. We have to collect the
         * work first, because some filters initialization depends on the input.
         */
        for (guint i = 0; i < num_inputs; i++) {
            work[i] = g_async_queue_pop(input_pop_queues[i]);

            if (work[i] == POISON_PILL) {
                g_async_queue_push(input_push_queues[i], POISON_PILL);
                state = FINISH;
                break;
            }

            timers[i] = ufo_buffer_get_transfer_timer(work[i]);
        } 

        if (state == FINISH) {
            push_poison_pill(producing_relations);
            break;
        }

        if (state == INIT && filter_class->initialize != NULL) {
            error = filter_class->initialize(filter, work, output_dims);
            state = WORK;

            if (error != NULL) {
                state = FINISH; 
                g_warning("%s", error->message);
                break;
            }

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

                    g_async_queue_push(output_pop_queues[port], buffer);
                } 
            }
        }

        /* 
         * Fetch buffers for output.
         */
        for (guint port = 0; port < num_outputs; port++)
            result[port] = g_async_queue_pop(output_pop_queues[port]);

        /*
         * Do the actual processing.
         */
        if (filter_class->process_gpu != NULL)
            error = filter_class->process_gpu(filter, work, result, info->cmd_queues[0]);
        else
            error = filter_class->process_cpu(filter, work, result, info->cmd_queues[0]);

        /*
         * Release any work items so that preceding nodes can re-use
         * them.
         */
        for (guint port = 0; port < num_inputs; port++) {
            g_async_queue_push(input_push_queues[port], work[port]); 
        }

        /*
         * If this filter is a pure producing node (e.g. file readers),
         * we need to check that it is finished and either push forward
         * its output or terminate execution by sending the poison pill.
         */
        if (!ufo_filter_is_finished (filter)) {
            for (guint port = 0; port < num_outputs; port++)
                g_async_queue_push (output_push_queues[port], result[port]);
        }
        else if (num_inputs == 0) {
            push_poison_pill (producing_relations);
            state = FINISH;

            /* now kill the result buffers */
            for (guint i = 0; i < num_outputs; i++)
                g_object_unref (result[i]);
            break;
        }
    }

    for (guint i = 0; i < num_outputs; i++) {
        UfoBuffer *buffer = g_async_queue_pop (output_pop_queues[0]);

        if (buffer != GINT_TO_POINTER (0x1))
            g_object_unref (buffer);
    }

    g_free (input_dims);
    g_free (output_dims);
    g_free (work);
    g_free (result);
    g_free (timers);
    g_free (input_pop_queues);
    g_free (input_push_queues);
    g_free (output_pop_queues);
    g_free (output_push_queues);
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
