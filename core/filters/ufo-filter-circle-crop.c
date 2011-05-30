#include <gmodule.h>
#include <CL/cl.h>
#include <math.h>

#include "ufo-resource-manager.h"
#include "ufo-filter-circle-crop.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"

struct _UfoFilterCircleCropPrivate {
    /* add your private data here */
    /* cl_kernel kernel; */
    float example;
};

GType ufo_filter_circle_crop_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(UfoFilterCircleCrop, ufo_filter_circle_crop, UFO_TYPE_FILTER);

#define UFO_FILTER_CIRCLE_CROP_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_CIRCLE_CROP, UfoFilterCircleCropPrivate))

enum {
    PROP_0,
    PROP_EXAMPLE, /* remove this or add more */
    N_PROPERTIES
};

static GParamSpec *circle_crop_properties[N_PROPERTIES] = { NULL, };

static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

/* 
 * virtual methods 
 */
static void ufo_filter_circle_crop_initialize(UfoFilter *filter)
{
    /* Here you can code, that is called for each newly instantiated filter */
    /*
    UfoFilterCircleCrop *self = UFO_FILTER_CIRCLE_CROP(filter);
    UfoResourceManager *manager = ufo_resource_manager();
    GError *error = NULL;
    self->priv->kernel = NULL;

    ufo_resource_manager_add_program(manager, "foo-kernel-file.cl", &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
        return;
    }

    self->priv->kernel = ufo_resource_manager_get_kernel(manager, "foo-kernel", &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
    }
    */
}

/*
 * This is the main method in which the filter processes one buffer after
 * another.
 */
static void ufo_filter_circle_crop_process(UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    GAsyncQueue *input_queue = ufo_element_get_input_queue(UFO_ELEMENT(filter));
    GAsyncQueue *output_queue = ufo_element_get_output_queue(UFO_ELEMENT(filter));
    cl_command_queue command_queue = (cl_command_queue) ufo_element_get_command_queue(UFO_ELEMENT(filter));

    UfoBuffer *input = (UfoBuffer *) g_async_queue_pop(input_queue);
    while (!ufo_buffer_is_finished(input)) {
        gint32 width, height;
        ufo_buffer_get_dimensions(input, &width, &height);

        float *data = ufo_buffer_get_cpu_data(input, command_queue);
        for (int x = 0; x < width; x++) {
            int x_off = x - width/2;
            x_off *= x_off;
            for (int y = 0; y < height; y++) {
                int y_off = y - height/2;
                y_off *= y_off;
                if (sqrt(x_off + y_off) > width/2)
                    data[y*width + x] = 0.0f;
            }
        }

        g_async_queue_push(output_queue, input);
        input = (UfoBuffer *) g_async_queue_pop(input_queue);
    }
    g_async_queue_push(output_queue, input);
}

static void ufo_filter_circle_crop_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterCircleCrop *self = UFO_FILTER_CIRCLE_CROP(object);

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

static void ufo_filter_circle_crop_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoFilterCircleCrop *self = UFO_FILTER_CIRCLE_CROP(object);

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

static void ufo_filter_circle_crop_class_init(UfoFilterCircleCropClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

    gobject_class->set_property = ufo_filter_circle_crop_set_property;
    gobject_class->get_property = ufo_filter_circle_crop_get_property;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;
    filter_class->initialize = ufo_filter_circle_crop_initialize;
    filter_class->process = ufo_filter_circle_crop_process;

    /* install properties */
    circle_crop_properties[PROP_EXAMPLE] = 
        g_param_spec_double("example",
            "This is an example property",
            "You should definately replace this with some meaningful property",
            -1.0,   /* minimum */
             1.0,   /* maximum */
             1.0,   /* default */
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_EXAMPLE, circle_crop_properties[PROP_EXAMPLE]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterCircleCropPrivate));
}

static void ufo_filter_circle_crop_init(UfoFilterCircleCrop *self)
{
    UfoFilterCircleCropPrivate *priv = self->priv = UFO_FILTER_CIRCLE_CROP_GET_PRIVATE(self);
    priv->example = 1.0;
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_CIRCLE_CROP, NULL);
}
