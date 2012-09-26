/**
 * SECTION:ufo-scheduler
 * @Short_description: Data transport between two UfoFilters
 * @Title: UfoScheduler
 */

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "ufo-aux.h"
#include "ufo-scheduler.h"
#include "ufo-configurable.h"
#include "ufo-filter.h"
#include "ufo-filter-source.h"
#include "ufo-filter-sink.h"
#include "ufo-filter-reduce.h"

G_DEFINE_TYPE_WITH_CODE (UfoScheduler, ufo_scheduler, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_CONFIGURABLE, NULL))

#define UFO_SCHEDULER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_SCHEDULER, UfoSchedulerPrivate))

typedef struct {
    UfoFilter          *filter;
    UfoProfiler        *profiler;
    guint               num_inputs;
    guint               num_outputs;
    UfoInputParameter  *input_params;
    UfoOutputParameter *output_params;
    guint             **output_dims;
    UfoBuffer         **work;
    UfoBuffer         **result;
    guint               num_children;
    GError             *error;
    GAsyncQueue        *return_queue;
} ThreadInfo;

struct _UfoSchedulerPrivate {
    UfoConfiguration    *config;
    UfoResourceManager  *manager;
    GAsyncQueue         *return_queue;
};

enum {
    PROP_0,
    PROP_CONFIG,
    PROP_RESOURCE_MANAGER,
    N_PROPERTIES
};

static GParamSpec *scheduler_properties[N_PROPERTIES] = { NULL, };

static gpointer NO_ERROR = GINT_TO_POINTER (1);

/**
 * ufo_scheduler_new:
 * @config: (allow-none): A #UfoConfiguration or %NULL
 * @manager: (allow-none): A #UfoResourceManager or %NULL
 *
 * Creates a new #UfoScheduler.
 *
 * Return value: A new #UfoScheduler
 */
UfoScheduler *
ufo_scheduler_new (UfoConfiguration    *config,
                   UfoResourceManager  *manager)
{
    UfoScheduler *scheduler;

    scheduler = UFO_SCHEDULER (g_object_new (UFO_TYPE_SCHEDULER,
                                             "configuration", config,
                                             "resource-manager", manager,
                                             NULL));

    return scheduler;
}

static void
alloc_output_buffers (ThreadInfo    *info,
                      gboolean       use_default_value,
                      gfloat         default_value)
{
    UfoFilter           *filter;
    UfoOutputParameter  *output_params;
    UfoResourceManager  *manager;
    guint num_outputs;
    guint num_buffers;

    filter = info->filter;
    output_params = ufo_filter_get_output_parameters (filter);
    manager = ufo_filter_get_resource_manager (filter);
    num_outputs = ufo_filter_get_num_outputs (filter);
    num_buffers = 2 * info->num_children * num_outputs;

    for (guint port = 0; port < num_outputs; port++) {
        UfoChannel *channel;
        guint num_dims;

        num_dims = output_params[port].n_dims;
        channel  = ufo_filter_get_output_channel (filter, port);

        for (guint i = 0; i < num_buffers; i++) {
            UfoBuffer *buffer;

            buffer = ufo_resource_manager_request_buffer (manager, num_dims, info->output_dims[port], NULL, NULL);

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

static gboolean
expected_number_of_items_fetched (UfoInputParameter *param)
{
    return param->n_expected_items != UFO_FILTER_INFINITE_INPUT &&
           param->n_fetched_items >= param->n_expected_items;
}

static void
cleanup_fetched (ThreadInfo *info)
{
    for (guint port = 0; port < info->num_inputs; port++) {
        if (expected_number_of_items_fetched (&info->input_params[port])) {
            UfoChannel *channel = ufo_filter_get_input_channel (info->filter, port);
            ufo_channel_release_input (channel, info->work[port]);
        }
    }
}

static gboolean
fetch_work (ThreadInfo *info)
{
    const gpointer POISON_PILL = GINT_TO_POINTER (1);
    gboolean success = TRUE;

    ufo_profiler_start (info->profiler, UFO_PROFILER_TIMER_FETCH);

    for (guint port = 0; port < info->num_inputs; port++) {
        UfoChannel *channel = ufo_filter_get_input_channel (info->filter, port);

        if (!expected_number_of_items_fetched (&info->input_params[port])) {
            info->work[port] = ufo_channel_fetch_input (channel);
            info->input_params[port].n_fetched_items++;
        }

        if (info->work[port] == POISON_PILL) {
            ufo_channel_finish (channel);
            success = FALSE;
        }
    }

    ufo_profiler_stop (info->profiler, UFO_PROFILER_TIMER_FETCH);
    return success;
}

static void
push_work (ThreadInfo *info)
{
    ufo_profiler_start (info->profiler, UFO_PROFILER_TIMER_RELEASE);

    for (guint port = 0; port < info->num_inputs; port++) {
        if (!expected_number_of_items_fetched (&info->input_params[port])) {
            UfoChannel *channel = ufo_filter_get_input_channel (info->filter, port);
            ufo_channel_release_input (channel, info->work[port]);
        }
    }

    ufo_profiler_stop (info->profiler, UFO_PROFILER_TIMER_RELEASE);
}

static void
fetch_result (ThreadInfo *info)
{
    ufo_profiler_start (info->profiler, UFO_PROFILER_TIMER_FETCH);

    for (guint port = 0; port < info->num_outputs; port++) {
        UfoChannel *channel = ufo_filter_get_output_channel (info->filter, port);
        info->result[port] = ufo_channel_fetch_output (channel);
    }

    ufo_profiler_stop (info->profiler, UFO_PROFILER_TIMER_FETCH);
}

static void
push_result (ThreadInfo *info)
{
    ufo_profiler_start (info->profiler, UFO_PROFILER_TIMER_RELEASE);

    for (guint port = 0; port < info->num_outputs; port++) {
        UfoChannel *channel = ufo_filter_get_output_channel (info->filter, port);
        ufo_channel_release_output (channel, info->result[port]);
    }

    ufo_profiler_stop (info->profiler, UFO_PROFILER_TIMER_RELEASE);
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

    alloc_output_buffers (info, FALSE, 0.0);

    while (cont) {
        fetch_result (info);

        cont = ufo_filter_source_generate (UFO_FILTER_SOURCE (filter), info->result, &error);

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
    UfoFilter       *filter = UFO_FILTER (info->filter);
    UfoFilterClass  *filter_class = UFO_FILTER_GET_CLASS (filter);
    GError          *error = NULL;
    gboolean         cont = TRUE;
    guint            iteration = 0;

    /*
     * Initialize
     */
    if (fetch_work (info)) {
        ufo_filter_initialize (filter, info->work, info->output_dims, &error);

        if (error != NULL) {
            g_error ("%s", error->message);
            return error;
        }

        alloc_output_buffers (info, FALSE, 0.0);
    }
    else {
        return NULL;
    }

    fetch_result (info);

    while (cont) {
        g_message ("%-25s processing % 5i",
                   ufo_filter_get_unique_name (filter),
                   iteration++);

        if (filter_class->process_gpu != NULL)
            ufo_filter_process_gpu (filter, info->work, info->result, &error);
        else
            ufo_filter_process_cpu (filter, info->work, info->result, &error);

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
        ufo_filter_sink_consume (sink_filter, info->work, &error);

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

        alloc_output_buffers (info, TRUE, default_value);
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
        ufo_filter_reduce_collect (UFO_FILTER_REDUCE (filter), info->work, info->result, &error);

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
        cont = ufo_filter_reduce_reduce (UFO_FILTER_REDUCE (filter),
                                         info->result,
                                         &error);

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
    ThreadInfo *info = (ThreadInfo *) data;
    UfoFilter *filter = info->filter;
    GError *error = NULL;

    g_assert (ufo_filter_get_command_queue (filter) != NULL);

    UfoOutputParameter *output_params = ufo_filter_get_output_parameters (filter);

    info->num_inputs    = ufo_filter_get_num_inputs (filter);
    info->num_outputs   = ufo_filter_get_num_outputs (filter);
    info->input_params  = ufo_filter_get_input_parameters (filter);
    info->output_params = ufo_filter_get_output_parameters (filter);
    info->work          = g_malloc (info->num_inputs * sizeof (UfoBuffer *));
    info->result        = g_malloc (info->num_outputs * sizeof (UfoBuffer *));
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

    g_async_queue_ref (info->return_queue);

    if (error != NULL) {
        GError *prefixed;

        g_propagate_prefixed_error (&prefixed, error, "%s: ", ufo_filter_get_plugin_name (info->filter));
        g_async_queue_push (info->return_queue, prefixed);
    }
    else
        g_async_queue_push (info->return_queue, NO_ERROR);

    g_async_queue_unref (info->return_queue);

    for (guint port = 0; port < info->num_outputs; port++) {
        UfoChannel *channel = ufo_filter_get_output_channel (info->filter, port);

        ufo_channel_finish (channel);
    }

    g_message ("UfoScheduler: %s finished", ufo_filter_get_unique_name (filter));

    g_free (info->output_dims);
    g_free (info->work);
    g_free (info->result);
    g_free (info);

    return NULL;
}

static void
print_row (const gchar *row, FILE *fp)
{
    fprintf (fp, "%s\n", row);
}

static void
pass_queue (UfoGraph *graph,
            UfoFilter *filter,
            guint queue,
            guint n_queues,
            cl_command_queue *queues)
{
    GList *children;
    guint i = queue;

    children = ufo_graph_get_children (graph, filter);

    for (GList *it = g_list_first (children); it != NULL; it = g_list_next (it), i++) {
        UfoFilter *successor;
        guint new_queue;

        successor = UFO_FILTER (it->data);
        new_queue = i % n_queues;
        ufo_filter_set_command_queue (successor, queues[new_queue]);
        pass_queue (graph, successor, new_queue, n_queues, queues);
    }
}

static void
init_filter_queues (UfoSchedulerPrivate *priv,
                    UfoGraph *graph)
{
    cl_command_queue *queues;
    GList *roots;
    guint n_queues;
    guint i = 0;

    ufo_resource_manager_get_command_queues (priv->manager, (gpointer *) &queues, &n_queues);

    roots = ufo_graph_get_roots (graph);

    for (GList *it = g_list_first (roots); it != NULL; it = g_list_next (it), i++) {
        UfoFilter *root;
        guint queue;

        root = UFO_FILTER (it->data);
        queue = i % n_queues;
        ufo_filter_set_command_queue (root, queues[queue]);
        pass_queue (graph, root, queue, n_queues, queues);
    }

    g_list_free (roots);
}

static GThread *
start_filter_thread (UfoSchedulerPrivate *priv,
                     UfoFilter *filter,
                     UfoGraph *graph,
                     GError **error)
{
    UfoProfilerLevel profile_level;
    GList *children;
    ThreadInfo *info;

    g_object_get (G_OBJECT (priv->config),
                  "profile-level", &profile_level,
                  NULL);

    info = g_new0 (ThreadInfo, 1);
    info->filter = filter;

    info->return_queue = priv->return_queue;
    info->profiler = ufo_profiler_new (profile_level);
    ufo_filter_set_profiler (filter, info->profiler);
    ufo_filter_set_resource_manager (filter, priv->manager);

    children = ufo_graph_get_children (graph, filter);
    info->num_children = g_list_length (children);
    g_list_free (children);

    return g_thread_create (process_thread, info, TRUE, error);
}

static FILE *
open_prefixed_file (const gchar *prefix, const gchar *suffix)
{
    gchar *filename;
    FILE *fp;

    filename = g_strdup_printf ("%s.%s", prefix, suffix);
    fp = fopen (filename, "w");
    g_free (filename);
    return fp;
}

static void
print_profile_information (UfoSchedulerPrivate *priv, UfoGraph *graph)
{
    GList *filters;
    UfoProfilerLevel profile_level;
    gchar *profile_prefix;
    FILE *opencl_fp = stdout;
    FILE *io_fp = stdout;
    FILE *sync_fp = stdout;

    g_object_get (G_OBJECT (priv->config),
                  "profile-level", &profile_level,
                  "profile-output-prefix", &profile_prefix,
                  NULL);

    if (profile_prefix != NULL) {
        if (profile_level & UFO_PROFILER_LEVEL_OPENCL)
            opencl_fp = open_prefixed_file (profile_prefix, "cl.stat");

        if (profile_level & UFO_PROFILER_LEVEL_IO)
            io_fp = open_prefixed_file (profile_prefix, "io.stat");

        if (profile_level & UFO_PROFILER_LEVEL_SYNC)
            sync_fp = open_prefixed_file (profile_prefix, "sync.stat");
    }

    filters = ufo_graph_get_filters (graph);

    for (GList *it = filters; it != NULL; it = g_list_next (it)) {
        UfoProfiler *profiler;
        UfoFilter *filter;

        filter = (UfoFilter *) it->data;
        profiler = ufo_filter_get_profiler (filter);

        if (profile_level & UFO_PROFILER_LEVEL_OPENCL)
            ufo_profiler_foreach (profiler, (UfoProfilerFunc) print_row, opencl_fp);

        if (profile_level & UFO_PROFILER_LEVEL_IO)
            fprintf (io_fp, "%-16s %f\n",
                     ufo_filter_get_plugin_name (filter),
                     ufo_profiler_elapsed (profiler, UFO_PROFILER_TIMER_IO));

        if (profile_level & UFO_PROFILER_LEVEL_SYNC)
            fprintf (sync_fp, "%-16s %f %f\n",
                     ufo_filter_get_plugin_name (filter),
                     ufo_profiler_elapsed (profiler, UFO_PROFILER_TIMER_FETCH),
                     ufo_profiler_elapsed (profiler, UFO_PROFILER_TIMER_RELEASE));

        g_object_unref (profiler);
    }
}

static GError *
spawn_threads (UfoSchedulerPrivate *priv, UfoGraph *graph)
{
    GList *filters;
    guint n_threads;
    GList *threads = NULL;
    GError *error = NULL;

    filters = ufo_graph_get_filters (graph);
    n_threads = g_list_length (filters);

    /* Start each filter in its own thread */
    for (GList *it = filters; it != NULL; it = g_list_next (it)) {
        UfoFilter *filter;
        GThread *thread;

        filter = (UfoFilter *) it->data;
        thread = start_filter_thread (priv, filter, graph, &error);

        if (error != NULL)
            return error;

        threads = g_list_append (threads, thread);
    }

    g_list_free (filters);
    g_list_free (threads);

    return NULL;
}

static GError *
join_threads (UfoSchedulerPrivate *priv, UfoGraph *graph)
{
    guint n_threads;
    guint n_finished;

    n_threads = ufo_graph_get_num_filters (graph);

    for (n_finished = 0; n_finished < n_threads; n_finished++) {
        gpointer val = g_async_queue_pop (priv->return_queue);

        if (val != NO_ERROR)
            return val;
    }

    return NULL;
}

/**
 * ufo_scheduler_run:
 * @scheduler: A #UfoScheduler object
 * @graph: A #UfoGraph object whose filters are scheduled
 * @error: return location for a GError with error codes from
 * #UfoPluginManagerError or %NULL
 *
 * Start executing all filters from the @filters list in their own threads.
 */
void
ufo_scheduler_run (UfoScheduler *scheduler,
                   UfoGraph     *graph,
                   GError      **error)
{
    UfoSchedulerPrivate *priv;

    GTimer      *timer;
    GError      *tmp_error = NULL;

    g_return_if_fail (UFO_IS_SCHEDULER (scheduler));

    priv = scheduler->priv;

    if (priv->config == NULL)
        priv->config = ufo_configuration_new ();

    if (priv->manager == NULL) {
        priv->manager = ufo_resource_manager_new (priv->config);
    }

    init_filter_queues (priv, graph);

    timer = g_timer_new ();
    tmp_error = spawn_threads (priv, graph);

    if (tmp_error != NULL) {
        g_propagate_error (error, tmp_error);
        return;
    }

    tmp_error = join_threads (priv, graph);

    if (tmp_error != NULL) {
        g_propagate_error (error, tmp_error);
        return;
    }

    g_timer_stop (timer);
    g_message ("Processing finished after %3.5f seconds", g_timer_elapsed (timer, NULL));
    g_timer_destroy (timer);

    print_profile_information (priv, graph);
}

static void
ufo_scheduler_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    UfoSchedulerPrivate *priv = UFO_SCHEDULER_GET_PRIVATE(object);

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
ufo_scheduler_get_property (GObject      *object,
                            guint         property_id,
                            GValue       *value,
                            GParamSpec   *pspec)
{
    UfoSchedulerPrivate *priv = UFO_SCHEDULER_GET_PRIVATE(object);

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
ufo_scheduler_dispose (GObject *object)
{
    UfoSchedulerPrivate *priv = UFO_SCHEDULER_GET_PRIVATE (object);

    ufo_unref_stored_object ((GObject **) &priv->config);
    ufo_unref_stored_object ((GObject **) &priv->manager);

    g_async_queue_unref (priv->return_queue);

    G_OBJECT_CLASS (ufo_scheduler_parent_class)->dispose (object);
    g_message ("UfoScheduler: disposed");
}

static void
ufo_scheduler_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_scheduler_parent_class)->finalize (object);
    g_message ("UfoScheduler: finalized");
}

static void
ufo_scheduler_class_init (UfoSchedulerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->set_property = ufo_scheduler_set_property;
    gobject_class->get_property = ufo_scheduler_get_property;
    gobject_class->dispose      = ufo_scheduler_dispose;
    gobject_class->finalize     = ufo_scheduler_finalize;

    scheduler_properties[PROP_RESOURCE_MANAGER] =
        g_param_spec_object ("resource-manager",
                             "A UfoResourceManager",
                             "A UfoResourceManager",
                             UFO_TYPE_RESOURCE_MANAGER,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    g_object_class_install_property (gobject_class, PROP_RESOURCE_MANAGER, scheduler_properties[PROP_RESOURCE_MANAGER]);
    g_object_class_override_property (gobject_class, PROP_CONFIG, "configuration");

    g_type_class_add_private (klass, sizeof (UfoSchedulerPrivate));
}

static void
ufo_scheduler_init (UfoScheduler *scheduler)
{
    UfoSchedulerPrivate *priv;
    scheduler->priv = priv = UFO_SCHEDULER_GET_PRIVATE (scheduler);
    priv->return_queue = g_async_queue_new ();
}
