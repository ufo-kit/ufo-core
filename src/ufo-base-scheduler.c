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
#include "ufo-aux.h"
#include "ufo-base-scheduler.h"
#include "ufo-configurable.h"
#include "ufo-filter.h"
#include "ufo-filter-source.h"
#include "ufo-filter-sink.h"
#include "ufo-filter-reduce.h"

G_DEFINE_TYPE_WITH_CODE (UfoBaseScheduler, ufo_base_scheduler, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_CONFIGURABLE, NULL))

#define UFO_BASE_SCHEDULER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BASE_SCHEDULER, UfoBaseSchedulerPrivate))

typedef struct {
    UfoFilter          *filter;
    guint               num_cmd_queues;
    cl_command_queue   *cmd_queues;
    guint               num_inputs;
    guint               num_outputs;
    UfoProfiler        *profiler;
    UfoInputParameter  *input_params;
    UfoOutputParameter *output_params;
    guint             **output_dims;
    UfoBuffer         **work;
    UfoBuffer         **result;
    GTimer            **timers;
    GTimer             *cpu_timer;
} ThreadInfo;

struct _UfoBaseSchedulerPrivate {
    UfoConfiguration    *config;
    UfoResourceManager  *manager;
};

enum {
    PROP_0,
    PROP_CONFIG,
    PROP_RESOURCE_MANAGER,
    N_PROPERTIES
};

static GParamSpec *base_scheduler_properties[N_PROPERTIES] = { NULL, };

/**
 * ufo_base_scheduler_new:
 * @config: (allow-none): A #UfoConfiguration or %NULL
 * @manager: (allow-none): A #UfoResourceManager or %NULL
 *
 * Creates a new #UfoBaseScheduler.
 *
 * Return value: A new #UfoBaseScheduler
 */
UfoBaseScheduler *
ufo_base_scheduler_new (UfoConfiguration    *config,
                        UfoResourceManager  *manager)
{
    UfoBaseScheduler *scheduler;

    scheduler = UFO_BASE_SCHEDULER (g_object_new (UFO_TYPE_BASE_SCHEDULER,
                                                  "configuration", config,
                                                  "resource-manager", manager,
                                                  NULL));

    return scheduler;
}

static void
alloc_output_buffers (UfoFilter     *filter,
                      guint        **output_dims,
                      gboolean       use_default_value,
                      gfloat         default_value)
{
    UfoOutputParameter *output_params = ufo_filter_get_output_parameters (filter);
    UfoResourceManager *manager = ufo_filter_get_resource_manager (filter);
    const guint num_outputs = ufo_filter_get_num_outputs (filter);

    for (guint port = 0; port < num_outputs; port++) {
        UfoChannel *channel;
        const guint num_dims = output_params[port].n_dims;

        channel = ufo_filter_get_output_channel (filter, port);

        for (guint i = 0; i < 3*num_outputs; i++) {
            UfoBuffer *buffer = ufo_resource_manager_request_buffer (manager, num_dims, output_dims[port], NULL, NULL);

            /*
             * For some reason, if we don't reference the buffer again,
             * it is returned by ufo_buffer_new() for other filters as
             * well, thus sharing the buffer which is not what we want.
             *
             * TODO: remove the reference in a meaningful way, or find
             * out what the problem is.
             */
            g_object_ref (buffer);

            if (use_default_value)
                ufo_buffer_fill_with_value (buffer, default_value);

            ufo_channel_insert (channel, buffer);
        }
    }
}

static void
cleanup_fetched (ThreadInfo *info)
{
    for (guint i = 0; i < info->num_inputs; i++) {
        if ((info->input_params[i].n_fetched_items == info->input_params[i].n_expected_items)) {
            UfoChannel *channel = ufo_filter_get_input_channel (info->filter, i);
            ufo_channel_release_input (channel, info->work[i]);
        }
    }
}

static gboolean
fetch_work (ThreadInfo *info)
{
    const gpointer POISON_PILL = GINT_TO_POINTER (1);
    gboolean success = TRUE;

    for (guint port = 0; port < info->num_inputs; port++) {
        UfoChannel *channel = ufo_filter_get_input_channel (info->filter, port);

        if ((info->input_params[port].n_expected_items == UFO_FILTER_INFINITE_INPUT) ||
            (info->input_params[port].n_fetched_items < info->input_params[port].n_expected_items))
        {
            info->work[port] = ufo_channel_fetch_input (channel);
            info->input_params[port].n_fetched_items++;
        }

        if (info->work[port] == POISON_PILL) {
            ufo_channel_finish (channel);
            success = FALSE;
        }
    }

    return success;
}

static void
push_work (ThreadInfo *info)
{
    for (guint port = 0; port < info->num_inputs; port++) {
        if ((info->input_params[port].n_expected_items == UFO_FILTER_INFINITE_INPUT) ||
            (info->input_params[port].n_fetched_items < info->input_params[port].n_expected_items))
        {
            UfoChannel *channel = ufo_filter_get_input_channel (info->filter, port);
            ufo_channel_release_input (channel, info->work[port]);
        }
    }
}

static void
fetch_result (ThreadInfo *info)
{
    for (guint port = 0; port < info->num_outputs; port++) {
        UfoChannel *channel = ufo_filter_get_output_channel (info->filter, port);

        info->result[port] = ufo_channel_fetch_output (channel);
    }
}

static void
push_result (ThreadInfo *info)
{
    for (guint port = 0; port < info->num_outputs; port++) {
        UfoChannel *channel = ufo_filter_get_output_channel (info->filter, port);

        ufo_channel_release_output (channel, info->result[port]);
    }
}

static GError *
process_source_filter (ThreadInfo *info)
{
    UfoFilter   *filter = UFO_FILTER (info->filter);
    GError      *error = NULL;
    gboolean     cont = TRUE;

    ufo_filter_source_initialize (UFO_FILTER_SOURCE (filter), info->output_dims, &error);

    if (error != NULL)
        return error;

    alloc_output_buffers (filter,
                          info->output_dims,
                          FALSE,
                          0.0);

    while (cont) {
        fetch_result (info);

        g_timer_continue (info->cpu_timer);
        cont = ufo_filter_source_generate (UFO_FILTER_SOURCE (filter), info->result, info->cmd_queues[0], &error);
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
    cl_command_queue queue;
    GError      *error = NULL;
    gboolean     cont = TRUE;
    guint        iteration = 0;

    /*
     * Initialize
     */
    if (fetch_work (info)) {
        ufo_filter_initialize (filter, info->work, info->output_dims, &error);

        if (error != NULL) {
            g_error ("%s", error->message);
            return error;
        }

        alloc_output_buffers (filter,
                              info->output_dims,
                              FALSE,
                              0.0);
    }
    else {
        return NULL;
    }

    fetch_result (info);
    queue = info->cmd_queues[0];

    while (cont) {
        g_message ("`%s-%p' processing item %i", ufo_filter_get_plugin_name (filter), (gpointer) filter, iteration++);

        if (filter_class->process_gpu != NULL) {
            g_timer_continue (info->cpu_timer);
            ufo_filter_process_gpu (filter, info->work, info->result, queue, &error);
            g_timer_stop (info->cpu_timer);
        }
        else {
            g_timer_continue (info->cpu_timer);
            ufo_filter_process_cpu (filter, info->work, info->result, queue, &error);
            g_timer_stop (info->cpu_timer);
        }

        if (error != NULL)
            return error;

        push_work (info);
        push_result (info);

        fetch_result (info);
        cont = fetch_work (info);
    }

    /*
     * In case this buffer keeps some of its inputs (if n_expected_items <
     * UFO_FILTER_INPUT_INFINITE), then we need to return those buffers to the
     * preceding filter. Otherwise we get stuck.
     */
    cleanup_fetched (info);

    return NULL;
}

static GError *
process_sink_filter (ThreadInfo *info)
{
    UfoFilter *filter = UFO_FILTER (info->filter);
    UfoFilterSink *sink_filter = UFO_FILTER_SINK (filter);
    GError *error = NULL;
    gboolean cont = TRUE;

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

        push_work (info);
        cont = fetch_work (info);
    }

    return NULL;
}

static GError *
process_reduce_filter (ThreadInfo *info)
{
    UfoFilter *filter = UFO_FILTER (info->filter);
    GError *error = NULL;
    gboolean cont;
    gfloat default_value = 0.0f;

    if (fetch_work (info)) {
        ufo_filter_reduce_initialize (UFO_FILTER_REDUCE (filter), info->work, info->output_dims, &default_value, &error);

        if (error != NULL)
            return error;

        alloc_output_buffers (filter,
                              info->output_dims,
                              TRUE,
                              default_value);
    }
    else {
        return NULL;
    }

    /*
     * Fetch the first result buffers and initialize them with the requested
     * default value. These buffer will be used throughout the collection phase.
     */
    fetch_result (info);

    for (guint i = 0; i < info->num_outputs; i++)
        ufo_buffer_fill_with_value (info->result[i], default_value);

    /*
     * Collect until no more data is available for consumption. The filter is
     * given the same result buffers, so that results can be accumulated.
     */
    cont = TRUE;

    while (cont) {
        g_timer_continue (info->cpu_timer);
        ufo_filter_reduce_collect (UFO_FILTER_REDUCE (filter), info->work, info->result, info->cmd_queues[0], &error);
        g_timer_stop (info->cpu_timer);

        if (error != NULL)
            return error;

        push_work (info);
        cont = fetch_work (info);
    }

    /*
     * Calculate reduction results until the filter finishes.
     */
    cont = TRUE;

    while (cont) {
        g_timer_continue (info->cpu_timer);
        cont = ufo_filter_reduce_reduce (UFO_FILTER_REDUCE (filter),
                                         info->result,
                                         info->cmd_queues[0],
                                         &error);
        g_timer_stop (info->cpu_timer);

        if (error != NULL)
            return error;

        if (cont) {
            push_result (info);
            fetch_result (info);
        }
    }

    return error;
}

static gpointer
process_thread (gpointer data)
{
    GError *error = NULL;
    ThreadInfo  *info = (ThreadInfo *) data;
    UfoFilter   *filter = info->filter;

    UfoOutputParameter *output_params = ufo_filter_get_output_parameters (filter);

    info->num_inputs    = ufo_filter_get_num_inputs (filter);
    info->num_outputs   = ufo_filter_get_num_outputs (filter);
    info->input_params  = ufo_filter_get_input_parameters (filter);
    info->output_params = ufo_filter_get_output_parameters (filter);
    info->work          = g_malloc (info->num_inputs * sizeof (UfoBuffer *));
    info->result        = g_malloc (info->num_outputs * sizeof (UfoBuffer *));
    info->timers        = g_malloc (info->num_inputs * sizeof (GTimer *));
    info->output_dims   = g_malloc0(info->num_outputs * sizeof (guint *));

    for (guint i = 0; i < info->num_inputs; i++)
        info->input_params[i].n_fetched_items = 0;

    for (guint i = 0; i < info->num_outputs; i++)
        info->output_dims[i] = g_malloc0 (output_params[i].n_dims * sizeof(guint));

    if (UFO_IS_FILTER_SOURCE (filter))
        error = process_source_filter (info);
    else if (UFO_IS_FILTER_SINK (filter))
        error = process_sink_filter (info);
    else if (UFO_IS_FILTER_REDUCE (filter))
        error = process_reduce_filter (info);
    else
        error = process_synchronous_filter (info);

    /*
     * In case of an error something really bad happened and data is corrupted
     * anyway, so don't bother with freeing resources. We need to show the error
     * ASAP...
     */
    if (error != NULL)
        return error;

    for (guint port = 0; port < info->num_outputs; port++) {
        UfoChannel *channel = ufo_filter_get_output_channel (info->filter, port);

        ufo_channel_finish (channel);
    }

    g_message ("UfoBaseScheduler: %s-%p finished", ufo_filter_get_plugin_name (filter), (gpointer) filter);

    g_free (info->output_dims);
    g_free (info->work);
    g_free (info->result);
    g_free (info->timers);

    return error;
}

static void
print_row (const gchar *row, gpointer user_data)
{
    g_print ("%s\n", row);
}

/**
 * ufo_base_scheduler_run:
 * @scheduler: A #UfoBaseScheduler object
 * @graph: A #UfoGraph object whose filters are scheduled
 * @error: return location for a GError with error codes from
 * #UfoPluginManagerError or %NULL
 *
 * Start executing all filters from the @filters list in their own threads.
 */
void
ufo_base_scheduler_run (UfoBaseScheduler    *scheduler,
                        UfoGraph            *graph,
                        GError             **error)
{
    UfoBaseSchedulerPrivate *priv;
    UfoResourceManager      *manager;
    cl_command_queue        *cmd_queues;
    GList       *filters;
    GTimer      *timer;
    GThread    **threads;
    GError      *tmp_error = NULL;
    ThreadInfo  *thread_info;
    guint        n_queues;
    guint        n_threads;
    guint        i = 0;

    g_return_if_fail (UFO_IS_BASE_SCHEDULER (scheduler));

    priv = scheduler->priv;
    filters = ufo_graph_get_filters (graph);
    manager = priv->manager;

    if (manager == NULL) {
        manager = ufo_resource_manager_new (priv->config);
        priv->manager = manager;
    }

    ufo_resource_manager_get_command_queues (manager,
                                             (gpointer *) &cmd_queues,
                                             &n_queues);

    n_threads   = g_list_length (filters);
    threads     = g_new0 (GThread *, n_threads);
    thread_info = g_new0 (ThreadInfo, n_threads);
    timer       = g_timer_new ();

    /* Start each filter in its own thread */
    for (GList *it = filters; it != NULL; it = g_list_next (it), i++) {
        UfoFilter *filter = UFO_FILTER (it->data);

        ufo_filter_set_profiler (filter, ufo_profiler_new ());
        ufo_filter_set_resource_manager (filter, manager);

        thread_info[i].profiler = ufo_profiler_new ();
        thread_info[i].filter = filter;
        thread_info[i].num_cmd_queues = n_queues;
        thread_info[i].cmd_queues = cmd_queues;

        ufo_filter_set_profiler (filter, thread_info[i].profiler);

        thread_info[i].cpu_timer = g_timer_new ();
        g_timer_stop (thread_info[i].cpu_timer);

        threads[i] = g_thread_create (process_thread, &thread_info[i], TRUE, &tmp_error);

        if (tmp_error != NULL) {
            /* FIXME: kill already started threads */
            g_propagate_error (error, tmp_error);
            return;
        }
    }

    /* Wait for them to finish */
    for (i = 0; i < n_threads; i++) {
        tmp_error = (GError *) g_thread_join (threads[i]);

        if (tmp_error != NULL) {
            g_propagate_error (error, tmp_error);
            return;
        }
    }

    /* Dump OpenCL events. */
    for (GList *it = filters; it != NULL; it = g_list_next (it)) {
        UfoProfiler *profiler;

        profiler = ufo_filter_get_profiler (UFO_FILTER (it->data));
        ufo_profiler_foreach (profiler, print_row, NULL);
        g_object_unref (profiler);
    }

    /* TODO: free the cpu timers */

    g_free (thread_info);
    g_free (threads);

    g_timer_stop (timer);
    g_message ("Processing finished after %3.5f seconds", g_timer_elapsed (timer, NULL));
    g_timer_destroy (timer);
}

static void
ufo_base_scheduler_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    UfoBaseSchedulerPrivate *priv = UFO_BASE_SCHEDULER_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_CONFIG:
            ufo_set_property_object ((GObject **) &priv->config, g_value_get_object (value));
            break;

        case PROP_RESOURCE_MANAGER:
            ufo_set_property_object ((GObject **) &priv->manager, g_value_get_object (value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_base_scheduler_get_property (GObject      *object,
                                 guint         property_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{
    UfoBaseSchedulerPrivate *priv = UFO_BASE_SCHEDULER_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_CONFIG:
            g_value_set_object (value, priv->config);
            break;

        case PROP_RESOURCE_MANAGER:
            g_value_set_object (value, priv->manager);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_base_scheduler_dispose (GObject *object)
{
    UfoBaseSchedulerPrivate *priv = UFO_BASE_SCHEDULER_GET_PRIVATE (object);

    ufo_unref_stored_object ((GObject **) &priv->config);
    ufo_unref_stored_object ((GObject **) &priv->manager);

    G_OBJECT_CLASS (ufo_base_scheduler_parent_class)->dispose (object);
    g_message ("UfoBaseScheduler: disposed");
}

static void
ufo_base_scheduler_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_base_scheduler_parent_class)->finalize (object);
    g_message ("UfoBaseScheduler: finalized");
}

static void
ufo_base_scheduler_class_init (UfoBaseSchedulerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->set_property = ufo_base_scheduler_set_property;
    gobject_class->get_property = ufo_base_scheduler_get_property;
    gobject_class->dispose      = ufo_base_scheduler_dispose;
    gobject_class->finalize     = ufo_base_scheduler_finalize;

    base_scheduler_properties[PROP_RESOURCE_MANAGER] =
        g_param_spec_object ("resource-manager",
                             "A UfoResourceManager",
                             "A UfoResourceManager",
                             UFO_TYPE_RESOURCE_MANAGER,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    g_object_class_install_property (gobject_class, PROP_RESOURCE_MANAGER, base_scheduler_properties[PROP_RESOURCE_MANAGER]);
    g_object_class_override_property (gobject_class, PROP_CONFIG, "configuration");

    g_type_class_add_private (klass, sizeof (UfoBaseSchedulerPrivate));
}

static void
ufo_base_scheduler_init (UfoBaseScheduler *base_scheduler)
{
    UfoBaseSchedulerPrivate *priv;
    base_scheduler->priv = priv = UFO_BASE_SCHEDULER_GET_PRIVATE (base_scheduler);
}
