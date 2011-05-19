#include <gmodule.h>
#include <CL/cl.h>

#include "ufo-resource-manager.h"
#include "ufo-filter-filter.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"

struct _UfoFilterFilterPrivate {
    cl_kernel kernel;
    float example;
};

GType ufo_filter_filter_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(UfoFilterFilter, ufo_filter_filter, UFO_TYPE_FILTER);

#define UFO_FILTER_FILTER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_FILTER, UfoFilterFilterPrivate))

enum {
    PROP_0,
    PROP_EXAMPLE, /* remove this or add more */
    N_PROPERTIES
};

static GParamSpec *filter_properties[N_PROPERTIES] = { NULL, };

static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

/* 
 * virtual methods 
 */
static void ufo_filter_filter_initialize(UfoFilter *filter)
{
    UfoFilterFilter *self = UFO_FILTER_FILTER(filter);
    UfoResourceManager *manager = ufo_resource_manager();
    GError *error = NULL;
    self->priv->kernel = NULL;

    ufo_resource_manager_add_program(manager, "filter.cl", &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
        return;
    }

    self->priv->kernel = ufo_resource_manager_get_kernel(manager, "filter", &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
    }
}

/*
 * This is the main method in which the filter processes one buffer after
 * another.
 */
static void ufo_filter_filter_process(UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_FILTER(filter));

    UfoFilterFilterPrivate *priv = UFO_FILTER_FILTER_GET_PRIVATE(filter);
    UfoResourceManager *manager = ufo_resource_manager();
    GAsyncQueue *input_queue = ufo_element_get_input_queue(UFO_ELEMENT(filter));
    GAsyncQueue *output_queue = ufo_element_get_output_queue(UFO_ELEMENT(filter));

    UfoBuffer *input = (UfoBuffer *) g_async_queue_pop(input_queue);
    gint32 width, height;
    ufo_buffer_get_dimensions(input, &width, &height); 
    g_message("buffer width %i", width);

    float *ramp_filter = g_malloc0(width * sizeof(float));
    const float scale = 2.0 / width / width;
    ramp_filter[0] = 0.0;
    ramp_filter[1] = 1.0 / width;
    for (int k = 1; k < width / 4; k++) {
        ramp_filter[2*k] = k*scale;
        ramp_filter[2*k + 1] = ramp_filter[2*k];
    }
    ramp_filter[width/2] = ramp_filter[1];
    for (int k = width/2; k < width; k += 2) {
        ramp_filter[k] = ramp_filter[width - k];
        ramp_filter[k + 1] = ramp_filter[width - k + 1];
    }

    UfoBuffer *filter_buffer = ufo_resource_manager_request_buffer(manager,
            width, 1, ramp_filter);
    cl_mem filter_mem = (cl_mem) ufo_buffer_get_gpu_data(filter_buffer);

    while (!ufo_buffer_is_finished(input)) {
        /* FIXME: width might change */

        cl_mem fft_buffer_mem = (cl_mem) ufo_buffer_get_gpu_data(input);
        cl_event event;
        size_t global_work_size[2];

        global_work_size[0] = width; /* this is twice the FFT size */
        global_work_size[1] = height;
        clSetKernelArg(priv->kernel, 0, sizeof(cl_mem), (void *) &fft_buffer_mem);
        clSetKernelArg(priv->kernel, 1, sizeof(cl_mem), (void *) &filter_mem);
        clEnqueueNDRangeKernel(ufo_buffer_get_command_queue(input), 
                priv->kernel, 
                2, NULL, global_work_size, NULL, 
                0, NULL, &event);
        
        ufo_buffer_wait_on_event(input, event);
        g_async_queue_push(output_queue, input);
        input = (UfoBuffer *) g_async_queue_pop(input_queue);
    }
    g_async_queue_push(output_queue, input);
}

static void ufo_filter_filter_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterFilter *self = UFO_FILTER_FILTER(object);

    /* Handle all properties accordingly */
    switch (property_id) {
        case PROP_EXAMPLE:
            self->priv->example = g_value_get_double(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_filter_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoFilterFilter *self = UFO_FILTER_FILTER(object);

    /* Handle all properties accordingly */
    switch (property_id) {
        case PROP_EXAMPLE:
            g_value_set_double(value, self->priv->example);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_filter_class_init(UfoFilterFilterClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

    gobject_class->set_property = ufo_filter_filter_set_property;
    gobject_class->get_property = ufo_filter_filter_get_property;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;
    filter_class->initialize = ufo_filter_filter_initialize;
    filter_class->process = ufo_filter_filter_process;

    /* install properties */
    filter_properties[PROP_EXAMPLE] = 
        g_param_spec_double("example",
            "This is an example property",
            "You should definately replace this with some meaningful property",
            -1.0,   /* minimum */
             1.0,   /* maximum */
             1.0,   /* default */
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_EXAMPLE, filter_properties[PROP_EXAMPLE]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterFilterPrivate));
}

static void ufo_filter_filter_init(UfoFilterFilter *self)
{
    UfoFilterFilterPrivate *priv = self->priv = UFO_FILTER_FILTER_GET_PRIVATE(self);
    priv->kernel = NULL;
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_FILTER, NULL);
}
