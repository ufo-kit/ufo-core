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

    guint               num_inputs;
    guint               num_outputs;
    GList               *_input_num_dims;
    GList               *_output_num_dims;

    cl_command_queue    command_queue;
    gfloat              cpu_time;
    gfloat              gpu_time;
    gboolean            done;
};


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
 * @UFO_FILTER_ERROR_INITIALIZATION: Filter could not be initialized.
 *
 * Possible errors that ufo_filter_connect_to() and ufo_filter_connect_by_name()
 * can return.
 */
GQuark ufo_filter_error_quark(void)
{
    return g_quark_from_static_string("ufo-filter-error-quark");
}

/**
 * ufo_filter_set_plugin_name:
 * @filter: A #UfoFilter.
 * @plugin_name: The name of this filter.
 *
 * Set the name of filter.
 */
void ufo_filter_set_plugin_name(UfoFilter *filter, const gchar *plugin_name)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    filter->priv->plugin_name = g_strdup(plugin_name);
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
/* void ufo_filter_connect_to(UfoFilter *source, UfoFilter *destination, GError **error) */
/* { */
/*     g_return_if_fail(UFO_IS_FILTER(source) || UFO_IS_FILTER(destination)); */
/*     GError *tmp_error = NULL; */
/*     GPtrArray *input_names = destination->priv->input_names; */
/*     GPtrArray *output_names = source->priv->output_names; */

/*     if (input_names->len < 1) { */
/*         g_set_error(error, UFO_FILTER_ERROR, UFO_FILTER_ERROR_INSUFFICIENTINPUTS, */
/*                 "%s does not provide any inputs", destination->priv->plugin_name); */
/*         return; */
/*     } */

/*     if (output_names->len < 1) { */
/*         g_set_error(error, UFO_FILTER_ERROR, UFO_FILTER_ERROR_INSUFFICIENTOUTPUTS, */
/*                 "%s does not provide any outputs", source->priv->plugin_name); */
/*         return; */
/*     } */

/*     ufo_filter_connect_by_name(source, g_ptr_array_index(output_names, 0), */ 
/*             destination, g_ptr_array_index(input_names, 0), &tmp_error); */

/*     if (tmp_error != NULL) */
/*         g_propagate_error(error, tmp_error); */ 
/* } */

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
/* void ufo_filter_connect_by_name(UfoFilter *source, const gchar *output_name, */ 
/*         UfoFilter *destination, const gchar *input_name, GError **error) */
/* { */
/*     g_return_if_fail(UFO_IS_FILTER(source) || UFO_IS_FILTER(destination)); */
/*     g_return_if_fail((output_name != NULL) || (input_name != NULL)); */

/*     GPtrArray *input_num_dims = destination->priv->input_num_dims; */
/*     GPtrArray *output_num_dims = source->priv->output_num_dims; */
/*     GPtrArray *input_names = destination->priv->input_names; */
/*     GPtrArray *output_names = source->priv->output_names; */

/*     gint source_pos = filter_find_argument_position(output_names, output_name); */
/*     if (source_pos < 0) { */
/*         g_set_error(error, UFO_FILTER_ERROR, UFO_FILTER_ERROR_NOSUCHOUTPUT, */
/*                 "%s does not provide output %s", source->priv->plugin_name, output_name); */
/*         return; */
/*     } */

/*     gint destination_pos = filter_find_argument_position(input_names, input_name); */
/*     if (destination_pos < 0) { */
/*         g_set_error(error, UFO_FILTER_ERROR, UFO_FILTER_ERROR_NOSUCHOUTPUT, */
/*                 "%s does not provide input %s", destination->priv->plugin_name, output_name); */
/*         return; */
/*     } */

/*     if (GPOINTER_TO_INT(g_ptr_array_index(input_num_dims, 0)) != GPOINTER_TO_INT(g_ptr_array_index(output_num_dims, 0))) { */
/*         g_set_error(error, UFO_FILTER_ERROR, UFO_FILTER_ERROR_NUMDIMSMISMATCH, */
/*                 "Number of dimensions of %s:%s and %s:%s do not match", */
/*                 source->priv->plugin_name, output_name, */
/*                 destination->priv->plugin_name, input_name); */ 
/*         return; */
/*     } */

/*     UfoChannel *channel_in = ufo_filter_get_input_channel_by_name(destination, input_name); */
/*     UfoChannel *channel_out = ufo_filter_get_output_channel_by_name(source, output_name); */

/*     if ((channel_in == NULL) && (channel_out == NULL)) { */
/*         UfoChannel *channel = ufo_channel_new(); */
/*         filter_set_output_channel(source, output_name, channel); */
/*         filter_set_input_channel(destination, input_name, channel); */
/*     } */
/*     else if (channel_in == NULL) */
/*         filter_set_input_channel(destination, input_name, channel_out); */
/*     else if (channel_out == NULL) */
/*         filter_set_output_channel(source, output_name, channel_in); */
/* } */

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

    priv->num_inputs++;
    priv->_input_num_dims = g_list_append(priv->_input_num_dims, GINT_TO_POINTER(num_dims));
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

    priv->num_outputs++;
    priv->_output_num_dims = g_list_append(priv->_output_num_dims, GINT_TO_POINTER(num_dims));
}

guint ufo_filter_get_num_inputs(UfoFilter *filter)
{
    return filter->priv->num_inputs;
}

guint ufo_filter_get_num_outputs(UfoFilter *filter)
{
    return filter->priv->num_outputs;
}

GList *ufo_filter_get_input_num_dims(UfoFilter *filter)
{
    g_return_val_if_fail(UFO_IS_FILTER(filter), NULL);
    return filter->priv->_input_num_dims;
}

GList *ufo_filter_get_output_num_dims(UfoFilter *filter)
{
    g_return_val_if_fail(UFO_IS_FILTER(filter), NULL);
    return filter->priv->_output_num_dims;
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
 * ufo_filter_done:
 * @filter: A #UfoFilter
 *
 * Pure producer filters have to call this method to signal that no more data
 * can be expected.
 */
void ufo_filter_done(UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    filter->priv->done = TRUE;
}

/**
 * ufo_filter_is_done:
 * @filter: A #UfoFilter
 *
 * Get information about the current execution status of a pure producer filter.
 * Any other filters are driven by their inputs and are implicitly taken as done
 * if no data is pushed into them.
 *
 * Return value: TRUE if no more data is pushed.
 */
gboolean ufo_filter_is_done(UfoFilter *filter)
{
    return filter->priv->done;
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
    priv->done = FALSE;
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

    priv->num_inputs = 0;
    priv->num_outputs = 0;
    priv->_input_num_dims = NULL;
    priv->_output_num_dims = NULL;
}

