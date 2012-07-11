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

    guint               num_inputs;
    guint               num_outputs;
    GList               *input_num_dims;
    GList               *output_num_dims;

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
 * @Varargs: a %NULL-terminated list of dimensionalities
 *
 * Specifies the number of dimensions for each input.
 *
 * Since: 0.2
 */
void
ufo_filter_register_inputs (UfoFilter *filter, ...)
{
    g_return_if_fail (UFO_IS_FILTER (filter));
    g_return_if_fail (filter->priv->num_inputs == 0);

    UfoFilterPrivate *priv = UFO_FILTER_GET_PRIVATE (filter);
    guint num_dims;
    va_list ap;
    va_start (ap, filter);
    priv->num_inputs = 0;

    va_start (ap, filter);

    while ((num_dims = va_arg (ap, guint)) != 0) {
        priv->num_inputs++;
        priv->input_num_dims = g_list_append (priv->input_num_dims, GINT_TO_POINTER (num_dims));
    }

    va_end (ap);
}

/**
 * ufo_filter_register_outputs:
 * @filter: A #UfoFilter.
 * @Varargs: a %NULL-terminated list of dimensionalities
 *
 * Specifies the number of dimensions for each output.
 *
 * Since: 0.2
 */
void
ufo_filter_register_outputs (UfoFilter *filter, ...)
{
    g_return_if_fail (UFO_IS_FILTER (filter));
    g_return_if_fail (filter->priv->num_outputs == 0);

    UfoFilterPrivate *priv = UFO_FILTER_GET_PRIVATE (filter);
    va_list ap;
    va_start (ap, filter);
    guint num_dims;
    priv->num_outputs = 0;

    va_start (ap, filter);

    while ((num_dims = va_arg (ap, guint)) != 0) {
        priv->num_outputs++;
        priv->output_num_dims = g_list_append (priv->output_num_dims, GINT_TO_POINTER (num_dims));
    }

    va_end (ap);
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
    return filter->priv->num_inputs;
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
    return filter->priv->num_outputs;
}

/**
 * ufo_filter_get_input_num_dims:
 * @filter: A #UfoFilter.
 *
 * Get a list with the number of dimensions for each input.
 *
 * Returns: (transfer none): A list with number of input dimensions.
 * Since: 0.2
 */
GList *
ufo_filter_get_input_num_dims (UfoFilter *filter)
{
    g_return_val_if_fail (UFO_IS_FILTER (filter), NULL);
    return filter->priv->input_num_dims;
}

/**
 * ufo_filter_get_output_num_dims:
 * @filter: A #UfoFilter.
 *
 * Get a list with the number of dimensions for each output.
 *
 * Returns: (transfer none): A list with number of output dimensions.
 * Since: 0.2
 */
GList *
ufo_filter_get_output_num_dims (UfoFilter *filter)
{
    g_return_val_if_fail (UFO_IS_FILTER (filter), NULL);
    return filter->priv->output_num_dims;
}

/**
 * ufo_filter_finish:
 * @filter: A #UfoFilter
 *
 * Pure producer filters have to call this method to signal that no more data
 * can be expected.
 */
void
ufo_filter_finish (UfoFilter *filter)
{
    g_return_if_fail (UFO_IS_FILTER (filter));
    filter->priv->finished = TRUE;
}

/**
 * ufo_filter_is_finished:
 * @filter: A #UfoFilter
 *
 * Get information about the current execution status of a pure producer filter.
 * Any other filters are driven by their inputs and are implicitly taken as finished
 * if no data is pushed into them.
 *
 * Return value: TRUE if no more data is pushed.
 */
gboolean
ufo_filter_is_finished (UfoFilter *filter)
{
    return filter->priv->finished;
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

    g_list_free (priv->input_num_dims);
    g_list_free (priv->output_num_dims);
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
    priv->num_inputs = 0;
    priv->num_outputs = 0;
    priv->input_num_dims = NULL;
    priv->output_num_dims = NULL;
}

