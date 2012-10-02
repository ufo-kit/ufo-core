/**
 * SECTION:ufo-filter
 * @Short_description: Single unit of computation
 * @Title: UfoFilter
 *
 * The base class for processing nodes.
 */

#include <glib.h>
#include <gmodule.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include <string.h>

#include "config.h"
#include "ufo-filter.h"
#include "ufo-filter-source.h"
#include "ufo-filter-sink.h"

G_DEFINE_TYPE(UfoFilter, ufo_filter, G_TYPE_OBJECT)

#define UFO_FILTER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER, UfoFilterPrivate))

struct _UfoFilterPrivate {
    gchar               *plugin_name;
    gchar               *unique_name;

    guint               n_inputs;
    guint               n_outputs;

    UfoResourceManager  *manager;
    UfoProfiler         *profiler;
    UfoInputParameter   *input_parameters;
    UfoOutputParameter  *output_parameters;
    UfoChannel         **input_channels;
    UfoChannel         **output_channels;

    cl_command_queue    command_queue;
    gboolean            finished;
};

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
 * @UFO_FILTER_ERROR_INITIALIZATION: Filter could not be initialized.
 * @UFO_FILTER_ERROR_METHOD_NOT_IMPLEMENTED: A method, further specified in the
 *      message, is not implemented.
 *
 * Possible errors that ufo_filter_connect_to() and ufo_filter_connect_by_name()
 * can return.
 */
GQuark
ufo_filter_error_quark(void)
{
    return g_quark_from_static_string ("ufo-filter-error-quark");
}

/**
 * ufo_filter_initialize:
 * @filter: A #UfoFilter.
 * @input: An array of buffers for each input port
 * @output_dim_sizes: The size of each dimension for each output
 * @error: Location for #GError.
 *
 * This function calls the implementation for the virtual initialize method. The
 * filter can use the input buffers as a hint to setup its own internal
 * structures. Moreover, it needs to return size of each output dimension in
 * each port as specified with ufo_filter_register_outputs():
 * <programlisting>
 * // register a 1-dimensional and a 2-dimensional output in object::init
 * ufo_filter_register_outputs (self, 1, 2, NULL);
 *
 * // specify sizes in object::initialize
 * output_dim_sizes[0][0] = 1024;
 *
 * output_dim_sizes[1][0] = 640;
 * output_dim_sizes[1][1] = 480;
 * </programlisting>
 *
 * Since: 0.2
 */
void
ufo_filter_initialize (UfoFilter *filter, UfoBuffer *input[], guint **output_dim_sizes, GError **error)
{
    g_return_if_fail (UFO_IS_FILTER (filter));
    UFO_FILTER_GET_CLASS (filter)->initialize (filter, input, output_dim_sizes, error);
}

/**
 * ufo_filter_set_resource_manager:
 * @filter: A #UfoFilter.
 * @manager: A #UfoResourceManager
 *
 * Set the resource manager that this filter uses for requesting resources.
 *
 * Since: 0.2
 */
void ufo_filter_set_resource_manager (UfoFilter *filter,
                                      UfoResourceManager *manager)
{
    UfoFilterPrivate *priv;

    g_return_if_fail (UFO_IS_FILTER (filter) && UFO_IS_RESOURCE_MANAGER (manager));
    priv = filter->priv;
    priv->manager = manager;
}

/**
 * ufo_filter_get_resource_manager:
 * @filter: A #UfoFilter.
 *
 * Get the resource manager that this filter uses for requesting resources.
 *
 * Returns: (transfer full): A #UfoResourceManager
 * Since: 0.2
 */
UfoResourceManager *ufo_filter_get_resource_manager (UfoFilter *filter)
{
    g_return_val_if_fail (UFO_IS_FILTER (filter), NULL);
    return filter->priv->manager;
}

/**
 * ufo_filter_set_profiler:
 * @filter: A #UfoFilter.
 * @profiler: A #UfoProfiler
 *
 * Set this filter's profiler.
 *
 * Since: 0.2
 */
void ufo_filter_set_profiler (UfoFilter     *filter,
                              UfoProfiler   *profiler)
{
    UfoFilterPrivate *priv;

    g_return_if_fail (UFO_IS_FILTER (filter) && UFO_IS_PROFILER (profiler));
    priv = filter->priv;

    if (priv->profiler != NULL)
        g_object_unref (priv->profiler);

    g_object_ref (profiler);
    priv->profiler = profiler;
}

/**
 * ufo_filter_get_profiler:
 * @filter: A #UfoFilter.
 *
 * Get this filter's profiler.
 *
 * Returns: (transfer full): A #UfoProfiler
 * Since: 0.2
 */
UfoProfiler *ufo_filter_get_profiler (UfoFilter *filter)
{
    g_return_val_if_fail (UFO_IS_FILTER (filter), NULL);
    return filter->priv->profiler;
}

/**
 * ufo_filter_process_cpu:
 * @filter: A #UfoFilter.
 * @input: An array of buffers for each input port
 * @output: An array of buffers for each output port
 * @error: Location for #GError.
 *
 * Process input data from a buffer array on the CPU and put the results into
 * buffers in the #output array.
 *
 * Since: 0.2
 */
void
ufo_filter_process_cpu (UfoFilter *filter, UfoBuffer *input[], UfoBuffer *output[], GError **error)
{
    g_return_if_fail (UFO_IS_FILTER (filter));
    UFO_FILTER_GET_CLASS (filter)->process_cpu (filter, input, output, error);
}

/**
 * ufo_filter_process_gpu: (skip)
 * @filter: A #UfoFilter.
 * @input: An array of buffers for each input port
 * @output: An array of buffers for each output port
 * @error: Location for an error.
 *
 * Process input data from a buffer array on the GPU and put the results into
 * buffers in the #output array. For each enqueue command, a %cl_event object
 * should be created and put into a #UfoEventList that is returned at the end:
 * <programlisting>
 * UfoEventList *event_list = ufo_event_list_new (2);
 * cl_event *events = ufo_event_list_get_event_array (event_list);
 *
 * clEnqueueNDRangeKernel(..., 0, NULL, &events[0]);
 *
 * return event_list;
 * </programlisting>
 *
 * Since: 0.2
 */
void
ufo_filter_process_gpu (UfoFilter *filter, UfoBuffer *input[], UfoBuffer *output[], GError **error)
{
    g_return_if_fail (UFO_IS_FILTER (filter));
    UFO_FILTER_GET_CLASS (filter)->process_gpu (filter, input, output, error);
}

/**
 * ufo_filter_set_plugin_name:
 * @filter: A #UfoFilter.
 * @plugin_name: The name of this filter.
 *
 * Set the name of filter.
 */
void
ufo_filter_set_plugin_name (UfoFilter *filter, const gchar *plugin_name)
{
    UfoFilterPrivate *priv;

    g_return_if_fail (UFO_IS_FILTER (filter));

    priv = filter->priv;
    priv->plugin_name = g_strdup (plugin_name);
    priv->unique_name = g_strdup_printf ("%s-%p",
                                         plugin_name,
                                         (gpointer) filter);
}

/**
 * ufo_filter_register_inputs: (skip)
 * @filter: A #UfoFilter.
 * @n_inputs: Number of inputs
 * @input_parameters: An array of #UfoInputParameter structures
 *
 * Specifies the number of dimensions and expected number of data elements for
 * each input.
 *
 * Since: 0.2
 */
void
ufo_filter_register_inputs (UfoFilter *filter, guint n_inputs, UfoInputParameter *parameters)
{
    UfoFilterPrivate *priv;

    g_return_if_fail (UFO_IS_FILTER (filter));
    g_return_if_fail (parameters != NULL);

    priv = UFO_FILTER_GET_PRIVATE (filter);

    if (UFO_IS_FILTER_SOURCE (filter)) {
        g_warning ("%s is a source filter and cannot receive any inputs",
                   priv->plugin_name);
        return;
    }

    priv->n_inputs = n_inputs;
    priv->input_channels = g_new0 (UfoChannel *, n_inputs);
    priv->input_parameters = g_new0 (UfoInputParameter, n_inputs);
    g_memmove (priv->input_parameters, parameters, n_inputs * sizeof(UfoInputParameter));
}

/**
 * ufo_filter_register_outputs: (skip)
 * @filter: A #UfoFilter.
 * @n_outputs: Number of outputs
 * @output_parameters: An array of #UfoOutputParameter structures
 *
 * Specifies the number of dimensions for each output.
 *
 * Since: 0.2
 */
void
ufo_filter_register_outputs (UfoFilter *filter, guint n_outputs, UfoOutputParameter *parameters)
{
    UfoFilterPrivate *priv;

    g_return_if_fail (UFO_IS_FILTER (filter));
    g_return_if_fail (parameters != NULL);

    priv = UFO_FILTER_GET_PRIVATE (filter);

    if (UFO_IS_FILTER_SINK (filter)) {
        g_warning ("%s is a sink filter and cannnot output any data",
                   priv->plugin_name);
        return;
    }

    priv->n_outputs = n_outputs;
    priv->output_channels = g_new0 (UfoChannel *, n_outputs);
    priv->output_parameters = g_new0 (UfoOutputParameter, n_outputs);
    g_memmove (priv->output_parameters, parameters, n_outputs * sizeof(UfoOutputParameter));
}

/**
 * ufo_filter_get_input_parameters: (skip)
 * @filter: A #UfoFilter.
 *
 * Get input parameters.
 *
 * Return: An array of #UfoInputParameter structures. This array must not be
 *      freed.
 */
UfoInputParameter *
ufo_filter_get_input_parameters (UfoFilter *filter)
{
    g_return_val_if_fail (UFO_IS_FILTER (filter), NULL);
    return filter->priv->input_parameters;
}

/**
 * ufo_filter_get_output_parameters: (skip)
 * @filter: A #UfoFilter.
 *
 * Get ouput parameters.
 *
 * Return: An array of #UfoOuputParameter structures. This array must not be
 *      freed.
 */
UfoOutputParameter *
ufo_filter_get_output_parameters (UfoFilter *filter)
{
    g_return_val_if_fail (UFO_IS_FILTER (filter), NULL);
    return filter->priv->output_parameters;
}

/**
 * ufo_filter_get_num_inputs:
 * @filter: A #UfoFilter.
 *
 * Return the number of input ports.
 *
 * Returns: Number of input ports.
 * Since: 0.2
 */
guint
ufo_filter_get_num_inputs (UfoFilter *filter)
{
    g_return_val_if_fail (UFO_IS_FILTER (filter), 0);
    return filter->priv->n_inputs;
}

/**
 * ufo_filter_get_num_outputs:
 * @filter: A #UfoFilter.
 *
 * Return the number of output ports.
 *
 * Returns: Number of output ports.
 * Since: 0.2
 */
guint
ufo_filter_get_num_outputs (UfoFilter *filter)
{
    g_return_val_if_fail (UFO_IS_FILTER (filter), 0);
    return filter->priv->n_outputs;
}

/**
 * ufo_filter_get_plugin_name:
 * @filter: A #UfoFilter.
 *
 * Get canonical name of @filter.
 *
 * Returns: (transfer none): %NULL-terminated string owned by the filter.
 */
const gchar *
ufo_filter_get_plugin_name (UfoFilter *filter)
{
    g_return_val_if_fail (UFO_IS_FILTER (filter), NULL);
    return filter->priv->plugin_name;
}

/**
 * ufo_filter_get_unique_name:
 * @filter: A #UfoFilter
 *
 * Get unique filter name consisting of the plugin name as returned by
 * ufo_filter_get_plugin_name(), a dash `-' and the address of the filter
 * object. This can be useful to differentiate between several instances of the
 * same filter.
 *
 * Returns: (transfer none): %NULL-terminated string owned by the filter.
 */
const gchar*
ufo_filter_get_unique_name (UfoFilter *filter)
{
    g_return_val_if_fail (UFO_IS_FILTER (filter), NULL);
    return filter->priv->unique_name;
}

/**
 * ufo_filter_set_output_channel:
 * @filter: A #UfoFilter.
 * @port: Output port number
 * @channel: A #UfoChannel.
 *
 * Set a filter's output channel for a certain output port.
 *
 * Since: 0.2
 */
void
ufo_filter_set_output_channel (UfoFilter *filter, guint port, UfoChannel *channel)
{
    UfoFilterPrivate *priv;

    g_return_if_fail (UFO_IS_FILTER (filter) && UFO_IS_CHANNEL (channel));
    g_return_if_fail (port < filter->priv->n_outputs);

    priv = filter->priv;

    if (priv->output_channels[port] != NULL)
        g_object_unref (priv->output_channels[port]);

    g_object_ref (channel);
    priv->output_channels[port] = channel;
}

/**
 * ufo_filter_get_output_channel:
 * @filter: A #UfoFilter.
 * @port: Output port number
 *
 * Return a filter's output channel for a certain output port.
 *
 * Returns: (transfer none) (allow-none): The associated output channel.
 * Since: 0.2
 */
UfoChannel *
ufo_filter_get_output_channel (UfoFilter *filter, guint port)
{
    g_return_val_if_fail (UFO_IS_FILTER (filter), NULL);
    g_return_val_if_fail (port < filter->priv->n_outputs, NULL);
    return filter->priv->output_channels[port];
}

/**
 * ufo_filter_set_input_channel:
 * @filter: A #UfoFilter.
 * @port: input port number
 * @channel: A #UfoChannel.
 *
 * Set a filter's input channel for a certain input port.
 *
 * Since: 0.2
 */
void
ufo_filter_set_input_channel (UfoFilter *filter, guint port, UfoChannel *channel)
{
    UfoFilterPrivate *priv;

    g_return_if_fail (UFO_IS_FILTER (filter) && UFO_IS_CHANNEL (channel));
    g_return_if_fail (port < filter->priv->n_inputs);

    priv = filter->priv;

    if (priv->input_channels[port] != NULL)
        g_object_unref (priv->input_channels[port]);

    g_object_ref (channel);
    filter->priv->input_channels[port] = channel;
}

/**
 * ufo_filter_get_input_channel:
 * @filter: A #UfoFilter.
 * @port: input port number
 *
 * Return a filter's input channel for a certain input port.
 *
 * Returns: (transfer none) (allow-none): The associated input channel.
 * Since: 0.2
 */
UfoChannel *
ufo_filter_get_input_channel (UfoFilter *filter, guint port)
{
    g_return_val_if_fail (UFO_IS_FILTER (filter), NULL);
    g_return_val_if_fail (port < filter->priv->n_inputs, NULL);
    return filter->priv->input_channels[port];
}

/**
 * ufo_filter_set_command_queue:
 * @filter: A #UfoFilter.
 * @cmd_queue: A %cl_command_queue to be used for computation and data transfer
 *
 * Set the associated command queue.
 * Since: 0.2
 */
void
ufo_filter_set_command_queue (UfoFilter *filter,
                              gpointer   cmd_queue)
{
    g_return_if_fail (UFO_IS_FILTER (filter));
    filter->priv->command_queue = cmd_queue;
}

/**
 * ufo_filter_get_command_queue:
 * @filter: A #UfoFilter.
 *
 * Get the associated command queue.
 * Returns: A %cl_command_queue or %NULL
 * Since: 0.2
 */
gpointer
ufo_filter_get_command_queue (UfoFilter *filter)
{
    g_return_val_if_fail (UFO_IS_FILTER (filter), NULL);
    return filter->priv->command_queue;
}

/**
 * ufo_filter_wait_until: (skip)
 * @filter: A #UfoFilter
 * @pspec: The specification of the property
 * @condition: A condition function to wait until it is satisfied
 * @user_data: User data passed to the condition func
 *
 * Wait until a property specified by @pspec fulfills @condition.
 */
void
ufo_filter_wait_until (UfoFilter *filter, GParamSpec *pspec, UfoFilterConditionFunc condition, gpointer user_data)
{
    GValue val = {0,};
    g_value_init (&val, pspec->value_type);

    while (1) {
        g_object_get_property (G_OBJECT (filter), pspec->name, &val);

        if (condition (&val, user_data))
            break;

        g_usleep (10000);
    }

    g_value_unset (&val);
}

static void
ufo_filter_initialize_real (UfoFilter *filter, UfoBuffer *input[], guint **output_dim_sizes, GError **error)
{
    g_debug ("Virtual method `initialize' of %s not implemented\n",
             filter->priv->plugin_name);
}

static void
ufo_filter_dispose (GObject *object)
{
    UfoFilterPrivate *priv;

    priv = UFO_FILTER_GET_PRIVATE (object);

    if (priv->profiler != NULL) {
        g_object_unref (priv->profiler);
        priv->profiler = NULL;
    }

    for (guint port = 0; port < priv->n_inputs; port++) {
        if (priv->input_channels[port] != NULL)
            g_object_unref (priv->input_channels[port]);
    }

    for (guint port = 0; port < priv->n_outputs; port++) {
        if (priv->output_channels[port] != NULL)
            g_object_unref (priv->output_channels[port]);
    }

    G_OBJECT_CLASS (ufo_filter_parent_class)->dispose(object);
    g_message ("UfoFilter (%p): disposed", (gpointer) object);
}

static void
ufo_filter_finalize (GObject *object)
{
    UfoFilterPrivate *priv = UFO_FILTER_GET_PRIVATE (object);

    g_free (priv->input_parameters);
    g_free (priv->output_parameters);
    g_free (priv->input_channels);
    g_free (priv->output_channels);
    g_free (priv->plugin_name);
    g_free (priv->unique_name);

    G_OBJECT_CLASS (ufo_filter_parent_class)->finalize (object);
    g_message ("UfoFilter (%p): finalized", (gpointer) object);
}

static void
ufo_filter_class_init (UfoFilterClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = ufo_filter_finalize;
    gobject_class->dispose  = ufo_filter_dispose;

    klass->initialize = ufo_filter_initialize_real;
    klass->process_cpu = NULL;
    klass->process_gpu = NULL;

    g_type_class_add_private (klass, sizeof (UfoFilterPrivate));
}

static void
ufo_filter_init (UfoFilter *self)
{
    UfoFilterPrivate *priv;
    self->priv = priv = UFO_FILTER_GET_PRIVATE (self);
    priv->finished = FALSE;
    priv->n_inputs = 0;
    priv->n_outputs = 0;
    priv->input_parameters = NULL;
    priv->output_parameters = NULL;
    priv->input_channels = NULL;
    priv->output_channels = NULL;
    priv->profiler = NULL;
    priv->command_queue = NULL;
    priv->plugin_name = NULL;
    priv->unique_name = NULL;
}

