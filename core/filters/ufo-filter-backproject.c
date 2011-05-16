#include <gmodule.h>
#include <math.h>
#include <CL/cl.h>
#include <clFFT.h>

#include "ufo-filter-backproject.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"
#include "ufo-resource-manager.h"


struct _UfoFilterBackprojectPrivate {
    cl_kernel kernel;
    gint num_sinograms;
    float axis_position;
    float angle_step;
};

GType ufo_filter_backproject_get_type(void) G_GNUC_CONST;

/* Inherit from UFO_TYPE_FILTER */
G_DEFINE_TYPE(UfoFilterBackproject, ufo_filter_backproject, UFO_TYPE_FILTER);

#define UFO_FILTER_BACKPROJECT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_BACKPROJECT, UfoFilterBackprojectPrivate))

enum {
    PROP_0 = 0,
    PROP_AXIS_POSITION,
    PROP_ANGLE_STEP,
    PROP_NUM_SINOGRAMS,
    N_PROPERTIES
};

static GParamSpec *backproject_properties[N_PROPERTIES] = { NULL, };

static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

/* 
 * virtual methods 
 */
static void ufo_filter_backproject_initialize(UfoFilter *filter)
{
    UfoFilterBackproject *self = UFO_FILTER_BACKPROJECT(filter);
    UfoResourceManager *manager = ufo_resource_manager();
    GError *error = NULL;
    self->priv->kernel = NULL;

    ufo_resource_manager_add_program(manager, "backproject.cl", &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
        return;
    }

    self->priv->kernel = ufo_resource_manager_get_kernel(manager, "backproject", &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
    }
}

static void ufo_filter_backproject_process(UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    UfoFilterBackproject *self = UFO_FILTER_BACKPROJECT(filter);
    UfoFilterBackprojectPrivate *priv = UFO_FILTER_BACKPROJECT_GET_PRIVATE(self);
    UfoResourceManager *manager = ufo_resource_manager();
    GAsyncQueue *input_queue = ufo_element_get_input_queue(UFO_ELEMENT(filter));
    GAsyncQueue *output_queue = ufo_element_get_output_queue(UFO_ELEMENT(filter));

    const float offset_x = -413.5f;
    const float offset_y = -413.5f;
    gint32 width, num_projections;
    UfoBuffer *sinogram = (UfoBuffer *) g_async_queue_pop(input_queue);
    ufo_buffer_get_dimensions(sinogram, &width, &num_projections);

    /* create angle arrays */
    float *cos_tmp = g_malloc0(sizeof(float) * num_projections);
    float *sin_tmp = g_malloc0(sizeof(float) * num_projections);
    float *axes_tmp = g_malloc0(sizeof(float) * num_projections);

    float step = priv->angle_step;
    for (int i = 0; i < num_projections; i++) {
        cos_tmp[i] = cos(i*step);
        sin_tmp[i] = sin(i*step);
        axes_tmp[i] = priv->axis_position;
    }

    UfoBuffer *cos_buffer = ufo_resource_manager_request_buffer(manager, num_projections, 1, cos_tmp);
    UfoBuffer *sin_buffer = ufo_resource_manager_request_buffer(manager, num_projections, 1, sin_tmp);
    UfoBuffer *axes_buffer = ufo_resource_manager_request_buffer(manager, num_projections, 1, axes_tmp);

    cl_mem cos_mem = ufo_buffer_get_gpu_data(cos_buffer);
    cl_mem sin_mem = ufo_buffer_get_gpu_data(sin_buffer);
    cl_mem axes_mem = ufo_buffer_get_gpu_data(axes_buffer);

    g_free(cos_tmp);
    g_free(sin_tmp);
    g_free(axes_tmp);

    cl_int err = CL_SUCCESS;
    err = clSetKernelArg(priv->kernel, 0, sizeof(gint32), &num_projections);
    err = clSetKernelArg(priv->kernel, 1, sizeof(gint32), &width);
    err = clSetKernelArg(priv->kernel, 2, sizeof(gint32), &offset_x);
    err = clSetKernelArg(priv->kernel, 3, sizeof(gint32), &offset_y);
    err = clSetKernelArg(priv->kernel, 4, sizeof(cl_mem), (void *) &cos_mem);
    err = clSetKernelArg(priv->kernel, 5, sizeof(cl_mem), (void *) &sin_mem);
    err = clSetKernelArg(priv->kernel, 6, sizeof(cl_mem), (void *) &axes_mem);

    while (!ufo_buffer_is_finished(sinogram)) {
        if (priv->kernel != NULL) {
            size_t global_work_size[2];
            float *slice_data = g_malloc0(sizeof(float) * width * width);
            UfoBuffer *slice = ufo_resource_manager_request_buffer(manager,
                    width, width, slice_data);

            cl_mem sinogram_mem = (cl_mem) ufo_buffer_get_gpu_data(sinogram);
            cl_mem slice_mem = (cl_mem) ufo_buffer_get_gpu_data(slice);
            cl_event event;

            err = clSetKernelArg(priv->kernel, 7, sizeof(cl_mem), (void *) &sinogram_mem);
            err = clSetKernelArg(priv->kernel, 8, sizeof(cl_mem), (void *) &slice_mem);
            global_work_size[0] = width;
            global_work_size[1] = width;
            err = clEnqueueNDRangeKernel(ufo_buffer_get_command_queue(sinogram),
                                         priv->kernel,
                                         2, NULL, global_work_size, NULL,
                                         0, NULL, &event);
            ufo_buffer_wait_on_event(slice, event);
            g_async_queue_push(output_queue, slice);
        }
        else {
            g_message("no kernel");
        }
        ufo_resource_manager_release_buffer(manager, sinogram);
        sinogram = (UfoBuffer *) g_async_queue_pop(input_queue);
    }

    g_async_queue_push(output_queue, 
            ufo_resource_manager_request_finish_buffer(manager));
}

static void ufo_filter_backproject_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterBackproject *self = UFO_FILTER_BACKPROJECT(object);
    switch (property_id) {
        case PROP_NUM_SINOGRAMS:
            self->priv->num_sinograms = g_value_get_int(value);
            break;
        case PROP_AXIS_POSITION:
            self->priv->axis_position = (float) g_value_get_double(value);
            break;
        case PROP_ANGLE_STEP:
            self->priv->angle_step = (float) g_value_get_double(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_backproject_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoFilterBackproject *self = UFO_FILTER_BACKPROJECT(object);
    switch (property_id) {
        case PROP_NUM_SINOGRAMS:
            g_value_set_int(value, self->priv->num_sinograms);
            break;
        case PROP_AXIS_POSITION:
            g_value_set_double(value, (double) self->priv->axis_position);
            break;
        case PROP_ANGLE_STEP:
            g_value_set_double(value, (double) self->priv->angle_step);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_backproject_class_init(UfoFilterBackprojectClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

    gobject_class->set_property = ufo_filter_backproject_set_property;
    gobject_class->get_property = ufo_filter_backproject_get_property;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;
    filter_class->initialize = ufo_filter_backproject_initialize;
    filter_class->process = ufo_filter_backproject_process;

    /* install properties */
    backproject_properties[PROP_NUM_SINOGRAMS] = 
        g_param_spec_int("num-sinograms",
            "Number of sinograms",
            "Number of to process",
            -1,   /* minimum */
            8192,   /* maximum */
            1,   /* default */
            G_PARAM_READWRITE);

    backproject_properties[PROP_AXIS_POSITION] = 
        g_param_spec_double("axis-pos",
            "Position of rotation axis",
            "Position of rotation axis",
            -1000.0,   /* minimum */
            +1000.0,   /* maximum */
            0.0,   /* default */
            G_PARAM_READWRITE);

    backproject_properties[PROP_ANGLE_STEP] = 
        g_param_spec_double("angle-step",
            "Increment of angle in radians",
            "Increment of angle in radians",
            -G_PI,  /* minimum */
            +G_PI,  /* maximum */
            0.0,    /* default */
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_NUM_SINOGRAMS, backproject_properties[PROP_NUM_SINOGRAMS]);
    g_object_class_install_property(gobject_class, PROP_AXIS_POSITION, backproject_properties[PROP_AXIS_POSITION]);
    g_object_class_install_property(gobject_class, PROP_ANGLE_STEP, backproject_properties[PROP_ANGLE_STEP]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterBackprojectPrivate));
}

static void ufo_filter_backproject_init(UfoFilterBackproject *self)
{
    self->priv = UFO_FILTER_BACKPROJECT_GET_PRIVATE(self);
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_BACKPROJECT, NULL);
}
