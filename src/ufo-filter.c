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
#include <string.h>

#include "config.h"
#include "ufo-filter.h"
#include "ufo-filter-source.h"
#include "ufo-filter-sink.h"

G_DEFINE_TYPE(UfoFilter, ufo_filter, G_TYPE_OBJECT)

#define UFO_FILTER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER, UfoFilterPrivate))

struct _UfoFilterPrivate {
    gchar               *plugin_name;

    guint               n_inputs;
    guint               n_outputs;

    UfoInputParameter   *input_parameters;
    UfoOutputParameter  *output_parameters;

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
 * ufo_filter_process_cpu:
 * @filter: A #UfoFilter.
 * @input: An array of buffers for each input port
 * @output: An array of buffers for each output port
 * @cmd_queue: A %cl_command_queue object for ufo_buffer_get_host_array()
 * @error: Location for #GError.
 *
 * Process input data from a buffer array on the CPU and put the results into
 * buffers in the #output array.
 *
 * Since: 0.2
 */
void
ufo_filter_process_cpu (UfoFilter *filter, UfoBuffer *input[], UfoBuffer *output[], gpointer cmd_queue, GError **error)
{
    g_return_if_fail (UFO_IS_FILTER (filter));
    UFO_FILTER_GET_CLASS (filter)->process_cpu (filter, input, output, cmd_queue, error);
}

/**
 * ufo_filter_process_gpu:
 * @filter: A #UfoFilter.
 * @input: An array of buffers for each input port
 * @output: An array of buffers for each output port
 * @cmd_queue: A %cl_command_queue object for ufo_buffer_get_host_array()
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
 * Returns: A #UfoEventList object containing cl_events.
 * Since: 0.2
 */
UfoEventList *
ufo_filter_process_gpu (UfoFilter *filter, UfoBuffer *input[], UfoBuffer *output[], gpointer cmd_queue, GError **error)
{
    g_return_val_if_fail (UFO_IS_FILTER (filter), NULL);
    return UFO_FILTER_GET_CLASS (filter)->process_gpu (filter, input, output, cmd_queue, error);
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
    g_return_if_fail (UFO_IS_FILTER (filter));
    filter->priv->plugin_name = g_strdup (plugin_name);
}

/**
 * ufo_filter_register_inputs:
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

    if (UFO_IS_FILTER_SOURCE (filter))
        g_warning ("A source does not receive any inputs");

    priv = UFO_FILTER_GET_PRIVATE (filter);
    priv->n_inputs = n_inputs;
    priv->input_parameters = g_new0 (UfoInputParameter, n_inputs);
    g_memmove (priv->input_parameters, parameters, n_inputs * sizeof(UfoInputParameter));
}

/**
 * ufo_filter_register_outputs:
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

    if (UFO_IS_FILTER_SINK (filter))
        g_warning ("A sink does not output any data");

    priv = UFO_FILTER_GET_PRIVATE (filter);
    priv->n_outputs = n_outputs;
    priv->output_parameters = g_new0 (UfoOutputParameter, n_outputs);
    g_memmove (priv->output_parameters, parameters, n_outputs * sizeof(UfoOutputParameter));
}

/**
 * ufo_filter_get_input_parameters:
 * @filter: A #UfoFilter.
 *
 * Get input parameters.
 *
 * Return: An array of #UfoInputParameter structures. This array must not be
 *      freed.
 */
UfoInputParameter *ufo_filter_get_input_parameters (UfoFilter *filter)
{
    g_return_val_if_fail (UFO_IS_FILTER (filter), NULL);
    return filter->priv->input_parameters;
}

/**
 * ufo_filter_get_input_parameters:
 * @filter: A #UfoFilter.
 *
 * Get input parameters.
 *
 * Return: An array of #UfoInputParameter structures. This array must not be
 *      freed.
 */
UfoOutputParameter *ufo_filter_get_output_parameters (UfoFilter *filter)
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
 * Return value: (transfer none): NULL-terminated string owned by the filter
 */
const gchar *
ufo_filter_get_plugin_name (UfoFilter *filter)
{
    g_return_val_if_fail (UFO_IS_FILTER (filter), NULL);
    return filter->priv->plugin_name;
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
    g_debug ("%s->initialize() not implemented\n", filter->priv->plugin_name);
}

static void
ufo_filter_finalize (GObject *object)
{
    UfoFilterPrivate *priv = UFO_FILTER_GET_PRIVATE (object);

    g_free (priv->input_parameters);
    g_free (priv->output_parameters);
    g_free (priv->plugin_name);

    G_OBJECT_CLASS (ufo_filter_parent_class)->finalize (object);
}

static void
ufo_filter_class_init (UfoFilterClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = ufo_filter_finalize;
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
    priv->command_queue = NULL;
    priv->finished = FALSE;
    priv->n_inputs = 0;
    priv->n_outputs = 0;
    priv->input_parameters = NULL;
    priv->output_parameters = NULL;
}

