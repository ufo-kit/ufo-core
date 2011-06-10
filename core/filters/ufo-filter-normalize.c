#include <gmodule.h>

#include "ufo-resource-manager.h"
#include "ufo-filter-normalize.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"

struct _UfoFilterNormalizePrivate {
    /* add your private data here */
    /* cl_kernel kernel; */
    float example;
};

GType ufo_filter_normalize_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(UfoFilterNormalize, ufo_filter_normalize, UFO_TYPE_FILTER);

#define UFO_FILTER_NORMALIZE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_NORMALIZE, UfoFilterNormalizePrivate))

enum {
    PROP_0,
    PROP_EXAMPLE, /* remove this or add more */
    N_PROPERTIES
};

static GParamSpec *normalize_properties[N_PROPERTIES] = { NULL, };

static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

/* 
 * virtual methods 
 */
static void ufo_filter_normalize_initialize(UfoFilter *filter)
{
    /* Here you can code, that is called for each newly instantiated filter */
    /*
    UfoFilterNormalize *self = UFO_FILTER_NORMALIZE(filter);
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
static void ufo_filter_normalize_process(UfoFilter *filter)
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

        float min = 1.0, max = 0.0;
        for (int i = 0; i < width*height; i++) {
            if (data[i] < min)
                min = data[i];
            if (data[i] > max)
                max = data[i];
        }

        float scale = 1.0 / (max - min);
        for (int i = 0; i < width*height; i++) {
            data[i] = (data[i] - min) * scale;
        }
        g_async_queue_push(output_queue, input);
        input = (UfoBuffer *) g_async_queue_pop(input_queue);
    }
    g_async_queue_push(output_queue, input);
}

static void ufo_filter_normalize_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterNormalize *self = UFO_FILTER_NORMALIZE(object);

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

static void ufo_filter_normalize_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoFilterNormalize *self = UFO_FILTER_NORMALIZE(object);

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

static void ufo_filter_normalize_class_init(UfoFilterNormalizeClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

    gobject_class->set_property = ufo_filter_normalize_set_property;
    gobject_class->get_property = ufo_filter_normalize_get_property;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;
    filter_class->initialize = ufo_filter_normalize_initialize;
    filter_class->process = ufo_filter_normalize_process;

    /* install properties */
    normalize_properties[PROP_EXAMPLE] = 
        g_param_spec_double("example",
            "This is an example property",
            "You should definately replace this with some meaningful property",
            -1.0,   /* minimum */
             1.0,   /* maximum */
             1.0,   /* default */
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_EXAMPLE, normalize_properties[PROP_EXAMPLE]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterNormalizePrivate));
}

static void ufo_filter_normalize_init(UfoFilterNormalize *self)
{
    UfoFilterNormalizePrivate *priv = self->priv = UFO_FILTER_NORMALIZE_GET_PRIVATE(self);
    priv->example = 1.0;
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_NORMALIZE, NULL);
}
