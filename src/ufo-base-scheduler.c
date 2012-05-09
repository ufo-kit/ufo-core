/**
 * SECTION:ufo-base_scheduler
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

G_DEFINE_TYPE(UfoBaseScheduler, ufo_base_scheduler, G_TYPE_OBJECT)

#define UFO_BASE_SCHEDULER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BASE_SCHEDULER, UfoBaseSchedulerPrivate))

typedef struct {
    gfloat cpu_time; 
    gfloat gpu_time;
} ExecutionInformation;

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
UfoBaseScheduler *ufo_base_scheduler_new(void)
{
    UfoBaseScheduler *base_scheduler = UFO_BASE_SCHEDULER(g_object_new(UFO_TYPE_BASE_SCHEDULER, NULL));
    return base_scheduler;
}

void ufo_base_scheduler_add_filter(UfoBaseScheduler *scheduler, UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_BASE_SCHEDULER(scheduler));
    UfoBaseSchedulerPrivate *priv = UFO_BASE_SCHEDULER_GET_PRIVATE(scheduler);

    ExecutionInformation *info = g_malloc0(sizeof(ExecutionInformation));
    info->cpu_time = 0.0f;
    info->gpu_time = 0.0f;
    g_hash_table_insert(priv->exec_info, filter, info);
    g_object_ref(filter);
}

static gpointer process_thread(gpointer data)
{
    UfoFilter *filter = UFO_FILTER(data);
    UfoFilterClass *filter_class = UFO_FILTER_GET_CLASS(filter);
    enum { INIT, WORK, FINISH } state = INIT;

    if ((filter_class->process_cpu != NULL) || (filter_class->process_gpu != NULL)) {
        guint num_inputs = 0;
        guint num_outputs = 0;
        UfoChannel **input_channels = ufo_filter_get_input_channels(filter, &num_inputs);
        UfoChannel **output_channels = ufo_filter_get_output_channels(filter, &num_outputs);
        UfoBuffer **work = g_malloc(num_inputs * sizeof(UfoBuffer *));
        UfoBuffer **result = g_malloc(num_outputs * sizeof(UfoBuffer *));

        while (state != FINISH) {
            /*
             * Collect the data from all inputs to transfer it over to the
             * filter in one pass
             */
            for (guint i = 0; i < num_inputs; i++) {
                work[i] = ufo_channel_get_input_buffer(input_channels[i]);
                if (work[i] == NULL) {
                    state = FINISH;
                    break;
                }
            } 

            if (state != FINISH) {
                if (state == INIT && filter_class->initialize != NULL) {
                    filter_class->initialize(filter, work);
                    state = WORK;
                }

                for (guint i = 0; i < num_outputs; i++)
                    result[i] = ufo_channel_get_output_buffer(output_channels[i]);

                /* 
                 * At this point we can decide where to run the filter and also
                 * change command queues to distribute work across devices.
                 */
                cl_command_queue cmd_queue = ufo_filter_get_command_queue(filter);

                if (filter_class->process_gpu != NULL)
                    filter_class->process_gpu(filter, work, result, cmd_queue);
                else
                    filter_class->process_cpu(filter, work, result, cmd_queue);

                for (guint i = 0; i < num_inputs; i++)
                    ufo_channel_finalize_input_buffer(input_channels[i], work[i]);

                for (guint i = 0; i < num_outputs; i++)
                    ufo_channel_finalize_output_buffer(output_channels[i], result[i]);
            }
            else {
                for (guint i = 0; i < num_outputs; i++)
                    ufo_channel_finish(output_channels[i]);
            }
        }

        g_free(input_channels);
        g_free(output_channels);
        g_free(work);
        g_free(result);
    }
    else {
        if (filter_class->initialize != NULL)
            filter_class->initialize(filter, NULL);

        /*
         * This is still needed for filters that consume more than one input
         * buffer at the time (e.g. reduction filters, sinogram generator etc.)
         */
        ufo_filter_process(filter);
    }
    return NULL;
}

/**
 * ufo_base_scheduler_run:
 * @scheduler: A #UfoBaseScheduler object
 * @error: return location for a GError with error codes from
 * #UfoPluginManagerError or %NULL
 *
 * Start executing all filters in their own threads.
 */
void ufo_base_scheduler_run(UfoBaseScheduler *scheduler, GError **error)
{
    g_return_if_fail(UFO_IS_BASE_SCHEDULER(scheduler));
    UfoBaseSchedulerPrivate *priv = UFO_BASE_SCHEDULER_GET_PRIVATE(scheduler);

    cl_command_queue *cmd_queues;
    guint num_queues;
    ufo_resource_manager_get_command_queues(ufo_resource_manager(), (void **) &cmd_queues, &num_queues);

    /* Start each filter in its own thread */
    GList *filters = g_hash_table_get_keys(priv->exec_info);
    GSList *threads = NULL;
    GError *tmp_error = NULL;
    g_thread_init(NULL);

    GTimer *timer = g_timer_new();
    g_timer_start(timer);

    for (GList *it = filters; it != NULL; it = g_list_next(it)) {
        UfoFilter *filter= UFO_FILTER(it->data);
        ufo_filter_set_command_queue(filter, cmd_queues[0]);
        threads = g_slist_append(threads, g_thread_create(process_thread, filter, TRUE, &tmp_error));

        if (tmp_error != NULL) {
            /* FIXME: kill already started threads */
            g_propagate_error(error, tmp_error);
            return;
        }
    }

    g_list_free(filters);
    GSList *thread = threads;

    while (thread) {
        tmp_error = (GError *) g_thread_join(thread->data);
        if (tmp_error != NULL) {
            /* FIXME: wait for the rest? */
            g_propagate_error(error, tmp_error);
            return;
        }
        thread = g_slist_next(thread);
    }

    g_slist_free(threads);
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
    priv->exec_info = g_hash_table_new_full(g_direct_hash, g_direct_equal, g_object_unref, g_free);
}
