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
static void ufo_filter_scale_initialize(UfoFilter *filter, UfoResourceManager *manager)
{
    UfoFilterScale *self = UFO_FILTER_SCALE(filter);
    GError *error = NULL;
    self->priv->kernel = NULL;

    ufo_resource_manager_add_program(manager, "scale.cl", &error);
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
    GAsyncQueue *input_queue = ufo_element_get_input_queue(UFO_ELEMENT(filter));
    GAsyncQueue *output_queue = ufo_element_get_output_queue(UFO_ELEMENT(filter));

    g_message("[scale] waiting...");
    UfoBuffer *buffer = (UfoBuffer *) g_async_queue_pop(input_queue);
    g_message("[scale] received buffer at queue %p", input_queue);

    if (ufo_buffer_is_finished(buffer)) {
        g_async_queue_push(output_queue, buffer);
        return;
    }

    if (self->priv->kernel != NULL) {
        float scale = (float) self->priv->scale;
        gsize global_work_size[2];

        ufo_buffer_get_dimensions(buffer, 
                (gint32 *) &global_work_size[0], 
                (gint32 *) &global_work_size[1]);

        global_work_size[0] *= global_work_size[1];
        cl_mem buffer_mem = (cl_mem) ufo_buffer_get_gpu_data(buffer);
        cl_int err = CL_SUCCESS;
        cl_event event;

        err = clSetKernelArg(self->priv->kernel, 0, sizeof(float), &scale);
        err = clSetKernelArg(self->priv->kernel, 1, sizeof(cl_mem), (void *) &buffer_mem);
        err = clEnqueueNDRangeKernel(ufo_buffer_get_command_queue(buffer),
                                     self->priv->kernel,
                                     1, NULL, global_work_size, NULL,
                                     0, NULL, &event);
        ufo_buffer_wait_on_event(buffer, event);
    }

    g_message("[scale] send buffer to queue %p", output_queue);
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
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_SCALE, NULL);
}
