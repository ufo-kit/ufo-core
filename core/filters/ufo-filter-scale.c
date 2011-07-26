#include <gmodule.h>
#include <CL/cl.h>

#include "ufo-filter-scale.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"
#include "ufo-resource-manager.h"


struct _UfoFilterScalePrivate {
    gdouble scale;
    cl_kernel kernel;
};

GType ufo_filter_scale_get_type(void) G_GNUC_CONST;

/* Inherit from UFO_TYPE_FILTER */
G_DEFINE_TYPE(UfoFilterScale, ufo_filter_scale, UFO_TYPE_FILTER);

#define UFO_FILTER_SCALE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_SCALE, UfoFilterScalePrivate))

enum {
    PROP_0,
    PROP_SCALE
};

static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

/* 
 * virtual methods 
 */
static void ufo_filter_scale_initialize(UfoFilter *filter)
{
    UfoFilterScale *self = UFO_FILTER_SCALE(filter);
    UfoResourceManager *manager = ufo_resource_manager();
    GError *error = NULL;
    self->priv->kernel = NULL;

    ufo_resource_manager_add_program(manager, "scale.cl", NULL, &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
        return;
    }

    self->priv->kernel = ufo_resource_manager_get_kernel(manager, "scale", &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
    }
}

static void ufo_filter_scale_process(UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    UfoFilterScale *self = UFO_FILTER_SCALE(filter);
    UfoFilterScalePrivate *priv = UFO_FILTER_SCALE_GET_PRIVATE(filter);
    GAsyncQueue *input_queue = ufo_element_get_input_queue(UFO_ELEMENT(filter));
    GAsyncQueue *output_queue = ufo_element_get_output_queue(UFO_ELEMENT(filter));
    cl_command_queue command_queue = (cl_command_queue) ufo_element_get_command_queue(UFO_ELEMENT(filter));
    cl_event event;

    gint32 width, height;
    UfoBuffer *buffer = (UfoBuffer *) g_async_queue_pop(input_queue);
    size_t global_work_size[2];
    float scale = (float) priv->scale;

    while (!ufo_buffer_is_finished(buffer)) {
        if (priv->kernel != NULL) {
            ufo_buffer_get_dimensions(buffer, &width, &height);
            global_work_size[0] = (size_t) width;
            global_work_size[1] = (size_t) height;

            cl_mem buffer_mem = (cl_mem) ufo_buffer_get_gpu_data(buffer, command_queue);
            cl_int err = CL_SUCCESS;

            err |= clSetKernelArg(self->priv->kernel, 0, sizeof(float), &scale);
            err |= clSetKernelArg(self->priv->kernel, 1, sizeof(cl_mem), (void *) &buffer_mem);
            err |= clEnqueueNDRangeKernel(command_queue,
                priv->kernel,
                2, NULL, global_work_size, NULL,
                0, NULL, &event);

            ufo_filter_account_gpu_time(filter, (void **) &event);
        }
        g_async_queue_push(output_queue, buffer);
        buffer = (UfoBuffer *) g_async_queue_pop(input_queue);
    }
    g_message("ufo-filter-scale: 0s/%fs", ufo_filter_get_gpu_time(filter));
    g_async_queue_push(output_queue, buffer);
}

static void ufo_filter_scale_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterScale *self = UFO_FILTER_SCALE(object);

    switch (property_id) {
        case PROP_SCALE:
            self->priv->scale = g_value_get_double(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_scale_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoFilterScale *self = UFO_FILTER_SCALE(object);

    switch (property_id) {
        case PROP_SCALE:
            g_value_set_double(value, self->priv->scale);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_scale_class_init(UfoFilterScaleClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

    gobject_class->set_property = ufo_filter_scale_set_property;
    gobject_class->get_property = ufo_filter_scale_get_property;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;
    filter_class->initialize = ufo_filter_scale_initialize;
    filter_class->process = ufo_filter_scale_process;

    /* install properties */
    GParamSpec *pspec;

    pspec = g_param_spec_double("scale",
        "Scale",
        "Scale for each pixel",
        -1.0,   /* minimum */
         1.0,   /* maximum */
         1.0,   /* default */
        G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_SCALE, pspec);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterScalePrivate));
}

static void ufo_filter_scale_init(UfoFilterScale *self)
{
    UfoFilterScalePrivate *priv = self->priv = UFO_FILTER_SCALE_GET_PRIVATE(self);
    priv->scale = 1.0;
    priv->kernel = NULL;
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_SCALE, NULL);
}
