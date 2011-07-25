#include <gmodule.h>
#include <CL/cl.h>

#include "ufo-resource-manager.h"
#include "ufo-filter-filter.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"

typedef enum {
    FILTER_RAMP
} FilterType;

struct _UfoFilterFilterPrivate {
    cl_kernel kernel;
    FilterType filter_type;
};

GType ufo_filter_filter_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(UfoFilterFilter, ufo_filter_filter, UFO_TYPE_FILTER);

#define UFO_FILTER_FILTER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_FILTER, UfoFilterFilterPrivate))

enum {
    PROP_0,
    PROP_FILTER_TYPE,
    N_PROPERTIES
};

static GParamSpec *filter_properties[N_PROPERTIES] = { NULL, };

static UfoBuffer *filter_create_data(UfoFilterFilterPrivate *priv, guint32 width)
{
    float *filter = g_malloc0(width * sizeof(float));
    const float scale = 2.0 / width / width;
    filter[0] = 0.0;
    filter[1] = 1.0 / width;
    for (int k = 1; k < width / 4; k++) {
        filter[2*k] = k*scale;
        filter[2*k + 1] = filter[2*k];
    }

    /* Mirror filter */
    for (int k = width/2; k < width; k += 2) {
        filter[k] = filter[width - k];
        filter[k + 1] = filter[width - k + 1];
    }
    UfoBuffer *filter_buffer = ufo_resource_manager_request_buffer(
            ufo_resource_manager(), width, 1, filter, TRUE);
    g_free(filter);
    return filter_buffer;
}

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

    ufo_resource_manager_add_program(manager, "filter.cl", NULL, &error);
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
    cl_command_queue command_queue = (cl_command_queue) ufo_element_get_command_queue(UFO_ELEMENT(filter));

    UfoBuffer *input = (UfoBuffer *) g_async_queue_pop(input_queue);
    gint32 width, height;
    ufo_buffer_get_dimensions(input, &width, &height); 

    UfoBuffer *filter_buffer = filter_create_data(priv, width);
    cl_mem filter_mem = (cl_mem) ufo_buffer_get_gpu_data(filter_buffer, command_queue);

    GTimer *timer = g_timer_new();
    while (!ufo_buffer_is_finished(input)) {
        /* FIXME: width might change */
        cl_mem fft_buffer_mem = (cl_mem) ufo_buffer_get_gpu_data(input, command_queue);
        cl_event event;
        size_t global_work_size[2];

        global_work_size[0] = width; /* this is twice the FFT size */
        global_work_size[1] = height;
        clSetKernelArg(priv->kernel, 0, sizeof(cl_mem), (void *) &fft_buffer_mem);
        clSetKernelArg(priv->kernel, 1, sizeof(cl_mem), (void *) &filter_mem);
        clEnqueueNDRangeKernel(command_queue,
                priv->kernel, 
                2, NULL, global_work_size, NULL, 
                0, NULL, &event);

        ufo_filter_account_gpu_time(filter, (void **) &event);
        
        ufo_buffer_wait_on_event(input, event);
        g_timer_stop(timer);
        g_async_queue_push(output_queue, input);
        input = (UfoBuffer *) g_async_queue_pop(input_queue);
        g_timer_continue(timer);
    }
    g_message("ufo-filter-filter: %fs/%fs", 
            g_timer_elapsed(timer, NULL), ufo_filter_get_gpu_time(filter));
    g_timer_destroy(timer);
    ufo_resource_manager_release_buffer(manager, filter_buffer);
    g_async_queue_push(output_queue, input);
}

static void ufo_filter_filter_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    /*UfoFilterFilter *self = UFO_FILTER_FILTER(object);*/

    /* Handle all properties accordingly */
    switch (property_id) {
        case PROP_FILTER_TYPE:
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
    /*UfoFilterFilter *self = UFO_FILTER_FILTER(object);*/

    /* Handle all properties accordingly */
    switch (property_id) {
        case PROP_FILTER_TYPE:
            g_value_set_string(value, "ramp");
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
    filter_properties[PROP_FILTER_TYPE] = 
        g_param_spec_string("filter-type",
            "Type of filter",
            "Type of filter",
            "ramp",
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_FILTER_TYPE, filter_properties[PROP_FILTER_TYPE]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterFilterPrivate));
}

static void ufo_filter_filter_init(UfoFilterFilter *self)
{
    UfoFilterFilterPrivate *priv = self->priv = UFO_FILTER_FILTER_GET_PRIVATE(self);
    priv->kernel = NULL;
    priv->filter_type = FILTER_RAMP;
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_FILTER, NULL);
}
