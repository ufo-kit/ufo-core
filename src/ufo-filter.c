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
    gfloat              cpu_time;
    gfloat              gpu_time;
    gboolean            done;
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

void ufo_filter_register_inputs(UfoFilter *filter, ...)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    g_return_if_fail(filter->priv->num_inputs == 0);

    UfoFilterPrivate *priv = UFO_FILTER_GET_PRIVATE(filter);
    guint num_dims;
    va_list ap;
    va_start(ap, filter);
    priv->num_inputs = 0;

    va_start(ap, filter);

    while ((num_dims = va_arg(ap, guint)) != 0) {
        priv->num_inputs++;
        priv->input_num_dims = g_list_append (priv->input_num_dims, GINT_TO_POINTER (num_dims));
    }

    va_end(ap);
} 

void ufo_filter_register_outputs(UfoFilter *filter, ...)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    g_return_if_fail(filter->priv->num_outputs == 0);

    UfoFilterPrivate *priv = UFO_FILTER_GET_PRIVATE(filter);    
    va_list ap;
    va_start(ap, filter);
    guint num_dims;
    priv->num_outputs = 0;

    va_start(ap, filter);

    while ((num_dims = va_arg(ap, guint)) != 0) {
        priv->num_outputs++;
        priv->output_num_dims = g_list_append (priv->output_num_dims, GINT_TO_POINTER (num_dims));
    }

    va_end(ap);
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
    return filter->priv->input_num_dims;
}

GList *ufo_filter_get_output_num_dims(UfoFilter *filter)
{
    g_return_val_if_fail(UFO_IS_FILTER(filter), NULL);
    return filter->priv->output_num_dims;
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
    priv->num_inputs = 0;
    priv->num_outputs = 0;
    priv->input_num_dims = NULL;
    priv->output_num_dims = NULL;
}

