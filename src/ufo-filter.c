/**
 * SECTION:ufo-filter
 * @Short_description: Single unit of computation
 * @Title: UfoFilter
 */

#include <glib.h>
#include <gmodule.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "config.h"
#include "ufo-filter.h"

G_DEFINE_TYPE(UfoFilter, ufo_filter, G_TYPE_OBJECT)

#define UFO_FILTER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER, UfoFilterPrivate))

struct _UfoFilterPrivate {
    gchar               *plugin_name;
    GPtrArray           *input_names;     /**< Contains *gchar */
    GPtrArray           *output_names;    /**< Contains *gchar */
    GPtrArray           *input_num_dims;  /**< Contains guint */ 
    GPtrArray           *output_num_dims; /**< Contains guint */  
    GHashTable          *output_channels; /**< Map from *char to *UfoChannel */
    GHashTable          *input_channels;  /**< Map from *char to *UfoChannel */
    cl_command_queue    command_queue;
    gfloat cpu_time;
    gfloat gpu_time;
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

static gint filter_find_argument_position(GPtrArray *array, const gchar *name)
{
    guint pos;
    
    for (pos = 0; pos < array->len; pos++) {
        if (!g_strcmp0(name, g_ptr_array_index(array, pos)))
            break;
    }
    
    return pos == array->len ? -1 : (gint) pos;
}

/**
 * UfoFilterError:
 * @UFO_FILTER_ERROR_INSUFFICIENTINPUTS: Filter does not provide enough inputs.
 * @UFO_FILTER_ERROR_INSUFFICIENTOUTPUTS: Filter does not provide enough
 *      outputs.
 * @UFO_FILTER_ERROR_NUMDIMSMISMATCH: Mismatch between number of dimensions of
 *      given input and output.
 * @UFO_FILTER_ERROR_NOSUCHINPUT: Filter does not provide an input with that
 *      name.
 * @UFO_FILTER_ERROR_NOSUCHOUTPUT: Filter does not provide an output with that
 *      name.
 *
 * Possible errors that ufo_filter_connect_to() and ufo_filter_connect_by_name()
 * can return.
 */
GQuark ufo_filter_error_quark(void)
{
    return g_quark_from_static_string("ufo-filter-error-quark");
}

/**
 * ufo_filter_initialize:
 * @filter: A #UfoFilter.
 * @plugin_name: The name of this filter.
 *
 * Initializes the concrete UfoFilter by giving it a name. This is necessary,
 * because we cannot instantiate the object on our own as this is already done
 * by the plugin manager.
 */
void ufo_filter_initialize(UfoFilter *filter, const gchar *plugin_name)
{
    UfoFilterPrivate *priv = UFO_FILTER_GET_PRIVATE(filter);
    priv->plugin_name = g_strdup(plugin_name);
}

/**
 * ufo_filter_get_input_channels:
 * @filter: A #UfoFilter.
 * @num_channels: Location for the number of returned channels
 *
 * Get the input channels associated with the filter.
 *
 * Returns: The input channels in "correct" order. Free the result with @g_free.
 */
UfoChannel **ufo_filter_get_input_channels(UfoFilter *filter, guint *num_channels)
{
    UfoFilterPrivate *priv = UFO_FILTER_GET_PRIVATE(filter);    
    const guint num_inputs = priv->input_names->len;
    UfoChannel **channels = g_malloc0(num_inputs * sizeof(UfoChannel*));

    for (guint i = 0; i < num_inputs; i++) {
        gchar *input_name = g_ptr_array_index(priv->input_names, i); 
        channels[i] = g_hash_table_lookup(priv->input_channels, input_name);
    }

    *num_channels = num_inputs;
    return channels;
}

/**
 * ufo_filter_get_output_channels:
 * @filter: A #UfoFilter.
 * @num_channels: Location for the number of returned channels
 *
 * Get the output channels associated with the filter.
 *
 * Returns: The output channels in "correct" order. Free the result with @g_free.
 */
UfoChannel **ufo_filter_get_output_channels(UfoFilter *filter, guint *num_channels)
{
    UfoFilterPrivate *priv = UFO_FILTER_GET_PRIVATE(filter);    
    const guint num_outputs = priv->output_names->len;
    UfoChannel **channels = g_malloc0(num_outputs * sizeof(UfoChannel*));

    for (guint i = 0; i < num_outputs; i++) {
        gchar *output_name = g_ptr_array_index(priv->output_names, i); 
        channels[i] = g_hash_table_lookup(priv->output_channels, output_name);
    }

    *num_channels = num_outputs;
    return channels;
}

/**
 * ufo_filter_process:
 * @filter: A #UfoFilter.
 *
 * Execute a filter.
 */
GError *ufo_filter_process(UfoFilter *filter)
{
    GError *error = NULL;

    if (UFO_FILTER_GET_CLASS(filter)->process != NULL) {
        GTimer *timer = g_timer_new();
        error = UFO_FILTER_GET_CLASS(filter)->process(filter);
        g_timer_stop(timer);
        filter->priv->cpu_time = (gfloat) g_timer_elapsed(timer, NULL);
        g_timer_destroy(timer);
    }
    else
        g_warning("%s::process not implemented", filter->priv->plugin_name);

    return error;
}

/**
 * ufo_filter_connect_to:
 * @source: Source #UfoFilter
 * @destination: Destination #UfoFilter
 * @error: Return location for error with values from #UfoFilterError
 *
 * Connect filter using the default first inputs and outputs.
 */
void ufo_filter_connect_to(UfoFilter *source, UfoFilter *destination, GError **error)
{
    g_return_if_fail(UFO_IS_FILTER(source) || UFO_IS_FILTER(destination));
    GError *tmp_error = NULL;
    GPtrArray *input_names = destination->priv->input_names;
    GPtrArray *output_names = source->priv->output_names;

    if (input_names->len < 1) {
        g_set_error(error, UFO_FILTER_ERROR, UFO_FILTER_ERROR_INSUFFICIENTINPUTS,
                "%s does not provide any inputs", destination->priv->plugin_name);
        return;
    }

    if (output_names->len < 1) {
        g_set_error(error, UFO_FILTER_ERROR, UFO_FILTER_ERROR_INSUFFICIENTOUTPUTS,
                "%s does not provide any outputs", source->priv->plugin_name);
        return;
    }

    ufo_filter_connect_by_name(source, g_ptr_array_index(output_names, 0), 
            destination, g_ptr_array_index(input_names, 0), &tmp_error);

    if (tmp_error != NULL)
        g_propagate_error(error, tmp_error); 
}

/**
 * ufo_filter_connect_by_name:
 * @source: Source #UfoFilter
 * @output_name: Name of the source output channel
 * @destination: Destination #UfoFilter
 * @input_name: Name of the destination input channel
 * @error: Return location for error with values from #UfoFilterError
 *
 * Connect output @output_name of filter @source with input @input_name of
 * filter @destination.
 */
void ufo_filter_connect_by_name(UfoFilter *source, const gchar *output_name, 
        UfoFilter *destination, const gchar *input_name, GError **error)
{
    g_return_if_fail(UFO_IS_FILTER(source) || UFO_IS_FILTER(destination));
    g_return_if_fail((output_name != NULL) || (input_name != NULL));

    GPtrArray *input_num_dims = destination->priv->input_num_dims;
    GPtrArray *output_num_dims = source->priv->output_num_dims;
    GPtrArray *input_names = destination->priv->input_names;
    GPtrArray *output_names = source->priv->output_names;

    gint source_pos = filter_find_argument_position(output_names, output_name);
    if (source_pos < 0) {
        g_set_error(error, UFO_FILTER_ERROR, UFO_FILTER_ERROR_NOSUCHOUTPUT,
                "%s does not provide output %s", source->priv->plugin_name, output_name);
        return;
    }

    gint destination_pos = filter_find_argument_position(input_names, input_name);
    if (destination_pos < 0) {
        g_set_error(error, UFO_FILTER_ERROR, UFO_FILTER_ERROR_NOSUCHOUTPUT,
                "%s does not provide input %s", destination->priv->plugin_name, output_name);
        return;
    }

    if (GPOINTER_TO_INT(g_ptr_array_index(input_num_dims, 0)) != GPOINTER_TO_INT(g_ptr_array_index(output_num_dims, 0))) {
        g_set_error(error, UFO_FILTER_ERROR, UFO_FILTER_ERROR_NUMDIMSMISMATCH,
                "Number of dimensions of %s:%s and %s:%s do not match",
                source->priv->plugin_name, output_name,
                destination->priv->plugin_name, input_name); 
        return;
    }

    UfoChannel *channel_in = ufo_filter_get_input_channel_by_name(destination, input_name);
    UfoChannel *channel_out = ufo_filter_get_output_channel_by_name(source, output_name);

    if ((channel_in == NULL) && (channel_out == NULL)) {
        UfoChannel *channel = ufo_channel_new();
        filter_set_output_channel(source, output_name, channel);
        filter_set_input_channel(destination, input_name, channel);
    }
    else if (channel_in == NULL)
        filter_set_input_channel(destination, input_name, channel_out);
    else if (channel_out == NULL)
        filter_set_output_channel(source, output_name, channel_in);
}

/**
 * ufo_filter_register_input:
 * @filter: A #UfoFilter
 * @name: Name of appended input
 * @num_dims: Number of dimensions this input accepts.
 *
 * Add a new input name. Each registered input is appended to the filter's
 * argument list.
 *
 * @note: This method must be called by each filter that accepts input.
 */
void ufo_filter_register_input(UfoFilter *filter, const gchar *name, guint num_dims)
{
    g_return_if_fail(UFO_IS_FILTER(filter) || (name != NULL));
    g_return_if_fail(num_dims <= UFO_BUFFER_MAX_NDIMS);
    UfoFilterPrivate *priv = UFO_FILTER_GET_PRIVATE(filter);

    if (filter_find_argument_position(priv->input_names, name) >= 0)
        return;

    g_ptr_array_add(priv->input_names, g_strdup(name));
    g_ptr_array_add(priv->input_num_dims, GINT_TO_POINTER(num_dims));
}

/**
 * ufo_filter_register_output:
 * @filter: A #UfoFilter
 * @name: Name of appended output 
 * @num_dims: Number of dimensions this output provides.
 *
 * Add a new output name. Each registered output is appended to the filter's
 * output list.
 *
 * @note: This method must be called by each filter that provides output.
 */
void ufo_filter_register_output(UfoFilter *filter, const gchar *name, guint num_dims)
{
    g_return_if_fail(UFO_IS_FILTER(filter) || (name != NULL));
    g_return_if_fail(num_dims <= UFO_BUFFER_MAX_NDIMS);
    UfoFilterPrivate *priv = UFO_FILTER_GET_PRIVATE(filter);    

    if (filter_find_argument_position(priv->output_names, name) >= 0)
        return;

    g_ptr_array_add(priv->output_names, g_strdup(name));
    g_ptr_array_add(priv->output_num_dims, GINT_TO_POINTER(num_dims));
}

/**
 * ufo_filter_connected:
 * @source: Source #UfoFilter.
 * @destination: Destination #UfoFilter.
 *
 * Check if @source and @destination are connected.
 *
 * Return value: TRUE if @source is connected with @destination else FALSE.
 */
gboolean ufo_filter_connected(UfoFilter *source, UfoFilter *destination)
{
    g_return_val_if_fail(UFO_IS_FILTER(source) || UFO_IS_FILTER(destination), FALSE);
    GList *output_channels = g_hash_table_get_values(source->priv->output_channels);
    GList *input_channels = g_hash_table_get_values(destination->priv->input_channels);

    for (guint i = 0; i < g_list_length(output_channels); i++) {
        gpointer channel = g_list_nth_data(output_channels, i);

        for (guint j = 0; j < g_list_length(input_channels); j++)
            if (channel == g_list_nth_data(input_channels, j))
                return TRUE;
    }

    return FALSE;
}

/**
 * ufo_filter_get_input_channel_by_name:
 * @filter: A #UfoFilter.
 * @name: Name of the input channel.
 *
 * Get input channel called @name from @filter.
 *
 * Return value: NULL if no such channel exists, otherwise the #UfoChannel object
 */
UfoChannel *ufo_filter_get_input_channel_by_name(UfoFilter *filter, const gchar *name)
{
    g_return_val_if_fail(UFO_IS_FILTER(filter) || (name != NULL), NULL);
    UfoChannel *channel = g_hash_table_lookup(filter->priv->input_channels, name);
    return channel;
}

/**
 * ufo_filter_get_output_channel_by_name:
 * @filter: A #UfoFilter.
 * @name: Name of the output channel.
 * Get named output channel
 *
 * Return value: NULL if no such channel exists, otherwise the #UfoChannel object
 */
UfoChannel *ufo_filter_get_output_channel_by_name(UfoFilter *filter, const gchar *name)
{
    g_return_val_if_fail(UFO_IS_FILTER(filter) || (name != NULL), NULL);
    UfoChannel *channel = g_hash_table_lookup(filter->priv->output_channels, name);
    return channel;
}

/**
 * ufo_filter_get_input_channel:
 * @filter: A #UfoFilter.
 *
 * Get default input channel
 *
 * Return value: NULL if no such channel exists, otherwise the #UfoChannel object.
 */
UfoChannel *ufo_filter_get_input_channel(UfoFilter *filter)
{
    g_return_val_if_fail(UFO_IS_FILTER(filter), NULL);
    UfoChannel *channel = g_hash_table_lookup(filter->priv->input_channels, 
            g_ptr_array_index(filter->priv->input_names, 0));
    return channel;
}

/**
 * ufo_filter_get_output_channel:
 * @filter: A #UfoFilter.
 *
 * Get default output channel of filter.
 *
 * Return value: NULL if no such channel exists, otherwise the #UfoChannel object.
 */
UfoChannel *ufo_filter_get_output_channel(UfoFilter *filter)
{
    g_return_val_if_fail(UFO_IS_FILTER(filter), NULL);
    UfoChannel *channel = g_hash_table_lookup(filter->priv->output_channels, 
            g_ptr_array_index(filter->priv->output_names, 0));
    return channel;
}

/**
 * ufo_filter_account_gpu_time:
 * @filter: A #UfoFilter.
 * @event: Pointer to a valid cl_event
 *
 * If profiling is enabled, it uses the event to account the execution time of
 * this event with this filter.
 */
void ufo_filter_account_gpu_time(UfoFilter *filter, gpointer event)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
#ifdef WITH_PROFILING
    cl_ulong start, end;
    cl_event e = (cl_event) event;
    clWaitForEvents(1, &e);
    clGetEventProfilingInfo(e, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
    clGetEventProfilingInfo(e, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
    filter->priv->gpu_time += (end - start) * 1e-9;
#endif
}

/**
 * ufo_filter_get_gpu_time:
 * @filter: A #UfoFilter
 *
 * Return value: Seconds that the filter used a GPU.
 */
float ufo_filter_get_gpu_time(UfoFilter *filter)
{
    g_return_val_if_fail(UFO_IS_FILTER(filter), 0.0f);
    return filter->priv->gpu_time;
}

/**
 * ufo_filter_get_plugin_name:
 * @filter: A #UfoFilter.
 *
 * Get canonical name of @filter.
 * Return value: (transfer none): NULL-terminated string owned by the filter
 */
const gchar *ufo_filter_get_plugin_name(UfoFilter *filter)
{
    g_return_val_if_fail(UFO_IS_FILTER(filter), NULL);
    return filter->priv->plugin_name;
}

/**
 * ufo_filter_set_command_queue:
 * @filter: A #UfoFilter.
 * @command_queue: A cl_command_queue to be associated with this filter.
 *
 * Set OpenCL command queue to use for OpenCL kernel invokations. The command
 * queue is usually set by UfoGraph and should not be changed by client code.
 */
void ufo_filter_set_command_queue(UfoFilter *filter, gpointer command_queue)
{
    g_return_if_fail(UFO_IS_FILTER(filter));

    if (filter->priv->command_queue == NULL)
        filter->priv->command_queue = command_queue;
}

/**
 * ufo_filter_get_command_queue:
 * @filter: A #UfoFilter.
 *
 * Get OpenCL command queue associated with a filter. This function should only
 * be called by a derived Filter implementation
 *
 * Return value: OpenCL command queue
 */
gpointer ufo_filter_get_command_queue(UfoFilter *filter)
{
    g_return_val_if_fail(UFO_IS_FILTER(filter), NULL);
    return filter->priv->command_queue;
}

/**
 * ufo_filter_set_gpu_affinity:
 * @filter: A #UfoFilter.
 * @gpu: Number of the preferred GPU.
 *
 * Select the GPU that this filter should use.
 */
void ufo_filter_set_gpu_affinity(UfoFilter *filter, guint gpu)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    UfoResourceManager *manager = ufo_resource_manager();
    guint num_queues = 0;
    cl_command_queue *cmd_queues = NULL;
    ufo_resource_manager_get_command_queues(manager, (void **) &cmd_queues, &num_queues);

    if (gpu < num_queues)
        filter->priv->command_queue = cmd_queues[gpu];
    else
        g_warning("Invalid GPU");
}

/**
 * ufo_filter_wait_until:
 * @filter: A #UfoFilter
 * @pspec: The specification of the property
 * @condition: A condition function to wait until it is satisfied
 * @user_data: User data passed to the condition func
 *
 * Wait until a property specified by @pspec fulfills @condition.
 */
void ufo_filter_wait_until(UfoFilter *filter, GParamSpec *pspec, UfoFilterConditionFunc condition, gpointer user_data)
{
    GValue val = {0,};
    g_value_init(&val, pspec->value_type);

    while (1) {
        g_object_get_property(G_OBJECT(filter), pspec->name, &val);

        if (condition(&val, user_data))
            break;

        g_usleep(10000);
    }

    g_value_unset(&val);
}

static void ufo_filter_finalize(GObject *object)
{
    UfoFilter *self = UFO_FILTER(object);
    UfoFilterPrivate *priv = UFO_FILTER_GET_PRIVATE(self);

#ifdef WITH_PROFILING
    g_message("Time for '%s'", priv->plugin_name);
    g_message("  GPU: %.4lfs", priv->gpu_time);
#endif

    g_ptr_array_free(priv->input_names, TRUE);
    g_ptr_array_free(priv->output_names, TRUE);
    g_ptr_array_free(priv->input_num_dims, FALSE);
    g_ptr_array_free(priv->output_num_dims, FALSE);

    g_hash_table_destroy(priv->input_channels);
    g_hash_table_destroy(priv->output_channels);
    g_free(priv->plugin_name);
    G_OBJECT_CLASS(ufo_filter_parent_class)->finalize(object);
}

static void ufo_filter_class_init(UfoFilterClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = ufo_filter_finalize;
    klass->initialize = NULL;
    klass->process = NULL;
    klass->process_cpu = NULL;
    klass->process_gpu = NULL;
    g_type_class_add_private(klass, sizeof(UfoFilterPrivate));
}

static void ufo_filter_init(UfoFilter *self)
{
    UfoFilterPrivate *priv;
    self->priv = priv = UFO_FILTER_GET_PRIVATE(self);
    priv->command_queue = NULL;
    priv->cpu_time = 0.0f;
    priv->gpu_time = 0.0f;
    priv->input_names = g_ptr_array_new_with_free_func(g_free);
    priv->output_names = g_ptr_array_new_with_free_func(g_free);
    priv->input_num_dims = g_ptr_array_new();
    priv->output_num_dims = g_ptr_array_new();
    priv->input_channels = g_hash_table_new_full(g_str_hash, g_str_equal,
                           g_free, NULL);
    priv->output_channels = g_hash_table_new_full(g_str_hash, g_str_equal,
                            g_free, (GDestroyNotify) g_object_unref);
}

