#include <glib.h>
#include <gmodule.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "config.h"
#include "ufo-filter.h"

G_DEFINE_TYPE(UfoFilter,
              ufo_filter, 
              ETHOS_TYPE_PLUGIN)

#define UFO_FILTER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER, UfoFilterPrivate))

enum {
    FINISHED,
    LAST_SIGNAL
};

struct _UfoFilterPrivate {
    GHashTable          *output_channels; /**< Map from *char to *UfoChannel */
    GHashTable          *input_channels;  /**< Map from *char to *UfoChannel */
    cl_command_queue    command_queue;
    gchar               *plugin_name;
    float cpu_time;
    float gpu_time;
};

static void filter_set_output_channel(UfoFilter *self, const gchar *name, UfoChannel *channel)
{
    UfoChannel *old_channel = g_hash_table_lookup(self->priv->output_channels, name);
    if (old_channel != NULL)
        g_hash_table_remove(self->priv->output_channels, name);

    g_hash_table_insert(self->priv->output_channels, g_strdup(name), channel);
    ufo_channel_ref(channel);
}

static void filter_set_input_channel(UfoFilter *self, const gchar *name, UfoChannel *channel)
{
    UfoChannel *old_channel = g_hash_table_lookup(self->priv->input_channels, name);
    if (old_channel != NULL)
        g_hash_table_remove(self->priv->input_channels, name);

    g_hash_table_insert(self->priv->input_channels, g_strdup(name), channel);
}


/* 
 * Public Interface
 */
/**
 * \brief Initializes the concrete UfoFilter with a UfoResourceManager. 
 * \public \memberof UfoFilter
 *
 * This is necessary, because we cannot instantiate the object on our own as
 * this is already done by the plugin manager.
 *
 * \param[in] filter A UfoFilter object
 */
void ufo_filter_initialize(UfoFilter *filter, const gchar *plugin_name)
{
    UfoFilterPrivate *priv = UFO_FILTER_GET_PRIVATE(filter);
    priv->plugin_name = g_strdup(plugin_name);
    if (UFO_FILTER_GET_CLASS(filter)->initialize != NULL)
        UFO_FILTER_GET_CLASS(filter)->initialize(filter);
}

/**
 * \brief Execute a particular filter.
 * \public \memberof UfoFilter
 *
 * \param filter A UfoFilter object
 */
void ufo_filter_process(UfoFilter *filter)
{
    if (UFO_FILTER_GET_CLASS(filter)->process != NULL) {
        GTimer *timer = g_timer_new();
        UFO_FILTER_GET_CLASS(filter)->process(filter);
        g_timer_stop(timer);
        filter->priv->cpu_time = g_timer_elapsed(timer, NULL);
        g_timer_destroy(timer);
    }
}

/**
 * \brief Connect filter using default input and output.
 * \public \memberof UfoFilter
 *
 * \param source Source UfoFilter
 * \param destination Destination Ufofilter
 */
void ufo_filter_connect_to(UfoFilter *source, UfoFilter *destination)
{
    ufo_filter_connect_by_name(source, "default", destination, "default");
}


/**
 * \brief Connect filter with named input and output
 * \public \memberof UfoFilter
 *
 * \param source Source UfoFilter
 * \param source_output Name of the source output channel
 * \param destination Destination Ufofilter
 * \param dest_input Name of the destination input channel
 */
void ufo_filter_connect_by_name(UfoFilter *source, const gchar *source_output, UfoFilter *destination, const gchar *dest_input)
{
    UfoChannel *channel_in = ufo_filter_get_input_channel_by_name(destination, dest_input); 
    UfoChannel *channel_out = ufo_filter_get_output_channel_by_name(source, source_output);
    
    if ((channel_in == NULL) && (channel_out == NULL)) {
        UfoChannel *channel = ufo_channel_new();
        filter_set_output_channel(source, source_output, channel);
        filter_set_input_channel(destination, dest_input, channel);
    }
    else if (channel_in == NULL)
        filter_set_input_channel(destination, dest_input, channel_out); 
    else if (channel_out == NULL)
        filter_set_output_channel(source, source_output, channel_in); 
}

/**
 * \brief Check if two filters are connected
 * \public \memberof UfoFilter
 *
 * \param source Source UfoFilter
 * \param destination Destination Ufofilter
 * \return TRUE if source is connected with destination else FALSE
 */
gboolean ufo_filter_connected(UfoFilter *source, UfoFilter *destination)
{
    GList *output_channels = g_hash_table_get_values(source->priv->output_channels);
    GList *input_channels = g_hash_table_get_values(destination->priv->input_channels);
    for (int i = 0; i < g_list_length(output_channels); i++) {
        gpointer channel = g_list_nth_data(output_channels, i);
        for (int j = 0; j < g_list_length(input_channels); j++)
            if (channel == g_list_nth_data(input_channels, j))
                return TRUE;
    }
    return FALSE;
}

/**
 * \brief Get named input channel
 * \public \memberof UfoFilter
 * 
 * \param filter UfoFilter object
 * \param name Name of the input channel
 * \return NULL if no such channel exists, otherwise pointer to UfoChannel object
 */
UfoChannel *ufo_filter_get_input_channel_by_name(UfoFilter *filter, const gchar *name)
{
    UfoChannel *channel = g_hash_table_lookup(filter->priv->input_channels, name);
    return channel;
}

/**
 * \brief Get named output channel
 * \public \memberof UfoFilter
 * 
 * \param filter UfoFilter object
 * \param name Name of the output channel
 * \return NULL if no such channel exists, otherwise pointer to UfoChannel object
 */
UfoChannel *ufo_filter_get_output_channel_by_name(UfoFilter *filter, const gchar *name)
{
    UfoChannel *channel = g_hash_table_lookup(filter->priv->output_channels, name);
    return channel;
}

/**
 * \brief Get default input channel
 * \public \memberof UfoFilter
 * 
 * \param filter UfoFilter object
 * \return NULL if no such channel exists, otherwise pointer to UfoChannel object
 */
UfoChannel *ufo_filter_get_input_channel(UfoFilter *filter)
{
    UfoChannel *channel = g_hash_table_lookup(filter->priv->input_channels, "default");
    return channel;
}

/**
 * \brief Get named output channel
 * \public \memberof UfoFilter
 * 
 * \param filter UfoFilter object
 * \return NULL if no such channel exists, otherwise pointer to UfoChannel object
 */
UfoChannel *ufo_filter_get_output_channel(UfoFilter *filter)
{
    UfoChannel *channel = g_hash_table_lookup(filter->priv->output_channels, "default");
    return channel;
}

void ufo_filter_account_gpu_time(UfoFilter *filter, void **event)
{
#ifdef WITH_PROFILING
    cl_ulong start, end;
    cl_event *e = (cl_event *) event;
    clWaitForEvents(1, e);
    clGetEventProfilingInfo(*e, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
    clGetEventProfilingInfo(*e, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
    filter->priv->gpu_time += (end - start) * 1e-9;
#endif
}

float ufo_filter_get_gpu_time(UfoFilter *filter)
{
    return filter->priv->gpu_time;
}

/**
 * \brief Get canonical name of the filter
 * \public \memberof UfoFilter
 * 
 * \param filter UfoFilter object
 * \return NUL-terminated string owned by the filter
 */
const gchar *ufo_filter_get_plugin_name(UfoFilter *filter)
{
    return filter->priv->plugin_name;
}

/**
 * \brief Set OpenCL command queue to use for OpenCL kernels
 * \public \memberof UfoFilter
 * \note The command queue is usually set by UfoGraph and should not be changed
 * by client code
 *
 * \param filter UfoFilter object
 */
void ufo_filter_set_command_queue(UfoFilter *filter, gpointer command_queue)
{
    if (filter->priv->command_queue == NULL)
        filter->priv->command_queue = command_queue;
}

/**
 * \brief Get OpenCL command queue associated with a filter
 * \public \memberof UfoFilter
 * \note This function should only be called by a derived Filter implementation
 * \param filter UfoFilter object
 * \return OpenCL command queue
 */
gpointer ufo_filter_get_command_queue(UfoFilter *filter)
{
    return filter->priv->command_queue;
}

void ufo_filter_set_gpu_affinity(UfoFilter *filter, guint gpu)
{
    UfoResourceManager *manager = ufo_resource_manager();
    int num_queues;
    cl_command_queue *cmd_queues;
    ufo_resource_manager_get_command_queues(manager, (void **) &cmd_queues, &num_queues); 
    if (gpu < num_queues)
        filter->priv->command_queue = cmd_queues[gpu];
    else
        g_warning("Invalid GPU");
}

/* 
 * Virtual Methods
 */

static void ufo_filter_dispose(GObject *object)
{
    UfoFilter *self = UFO_FILTER(object);
    UfoFilterPrivate *priv = UFO_FILTER_GET_PRIVATE(self);
#ifdef WITH_PROFILING
    g_message("Time for '%s'", priv->plugin_name);
    g_message("  GPU: %.4lfs", priv->gpu_time);
#endif

    /* Clears keys and unrefs channels */
    g_hash_table_destroy(priv->input_channels);
    g_hash_table_destroy(priv->output_channels);
    g_free(priv->plugin_name);
    G_OBJECT_CLASS(ufo_filter_parent_class)->dispose(object);
}


/*
 * Type/Class Initialization
 */
static void ufo_filter_class_init(UfoFilterClass *klass)
{
    /* override GObject methods */
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = ufo_filter_dispose;
    klass->initialize = NULL;
    klass->process = NULL;

    /* install private data */
    g_type_class_add_private(klass, sizeof(UfoFilterPrivate));
}

static void ufo_filter_init(UfoFilter *self)
{
    UfoFilterPrivate *priv;
    self->priv = priv = UFO_FILTER_GET_PRIVATE(self);
    priv->command_queue = NULL;
    priv->cpu_time = 0.0f;
    priv->gpu_time = 0.0f;
    priv->input_channels = g_hash_table_new_full(g_str_hash, g_str_equal,
            g_free, NULL);
    priv->output_channels = g_hash_table_new_full(g_str_hash, g_str_equal,
            g_free, (GDestroyNotify) g_object_unref);
}

