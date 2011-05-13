#include <gmodule.h>

#include "ufo-resource-manager.h"
#include "ufo-filter-sino-generator.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"

struct _UfoFilterSinoGeneratorPrivate {
    guint num_projections;
};

GType ufo_filter_sino_generator_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(UfoFilterSinoGenerator, ufo_filter_sino_generator, UFO_TYPE_FILTER);

#define UFO_FILTER_SINO_GENERATOR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_SINO_GENERATOR, UfoFilterSinoGeneratorPrivate))

enum {
    PROP_0,
    PROP_NUM_PROJECTIONS,
    N_PROPERTIES
};

static GParamSpec *sino_generator_properties[N_PROPERTIES] = { NULL, };

static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

/* 
 * virtual methods 
 */
static void ufo_filter_sino_generator_initialize(UfoFilter *filter)
{
    /* Here you can code, that is called for each newly instantiated filter */
}

/*
 * This is the main method in which the filter processes one buffer after
 * another.
 */
static void ufo_filter_sino_generator_process(UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    UfoFilterSinoGeneratorPrivate *priv = UFO_FILTER_SINO_GENERATOR_GET_PRIVATE(filter);
    GAsyncQueue *input_queue = ufo_element_get_input_queue(UFO_ELEMENT(filter));
    GAsyncQueue *output_queue = ufo_element_get_output_queue(UFO_ELEMENT(filter));

    /* We pop the very first image, to determine the size w*h of a projection.
     * We then have to allocate h sinogram buffers with a height of
     * num_projections and width w */
    gint32 width, height;
    g_message("[sino] wait ...");
    UfoBuffer *input = (UfoBuffer *) g_async_queue_pop(input_queue);
    g_message("[sino] got something");
    ufo_buffer_get_dimensions(input, &width, &height);

    g_message("[sino] generating %i sinograms with size %ix%i", 
        height, width, priv->num_projections);

    while (1) {
        /* We forward a finished buffer, to let succeeding filters know about
         * the end of computation. */
        if (ufo_buffer_is_finished(input)) {
            g_async_queue_push(output_queue, input);
            break;
        }

        /* Use the input here and push any output that's created. In the case of
         * a source filter, you wouldn't pop data from the input_queue but just
         * generate data with ufo_resource_manager_request_buffer() and push
         * that into output_queue. On the other hand, a sink filter would
         * release all incoming buffers from input_queue with
         * ufo_resource_manager_release_buffer() for further re-use. */

        g_async_queue_push(output_queue, input);
    }
}

static void ufo_filter_sino_generator_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterSinoGenerator *self = UFO_FILTER_SINO_GENERATOR(object);

    /* Handle all properties accordingly */
    switch (property_id) {
        case PROP_NUM_PROJECTIONS:
            self->priv->num_projections = g_value_get_int(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_sino_generator_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoFilterSinoGenerator *self = UFO_FILTER_SINO_GENERATOR(object);

    /* Handle all properties accordingly */
    switch (property_id) {
        case PROP_NUM_PROJECTIONS:
            g_value_set_int(value, self->priv->num_projections);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_sino_generator_class_init(UfoFilterSinoGeneratorClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

    gobject_class->set_property = ufo_filter_sino_generator_set_property;
    gobject_class->get_property = ufo_filter_sino_generator_get_property;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;
    filter_class->initialize = ufo_filter_sino_generator_initialize;
    filter_class->process = ufo_filter_sino_generator_process;

    /* install properties */
    sino_generator_properties[PROP_NUM_PROJECTIONS] = 
        g_param_spec_int("num-projections",
            "Number of projections",
            "Number of projections corresponding to the sinogram height",
            0,   /* minimum */
            8192,   /* maximum */
            1,   /* default */
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_NUM_PROJECTIONS, sino_generator_properties[PROP_NUM_PROJECTIONS]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterSinoGeneratorPrivate));
}

static void ufo_filter_sino_generator_init(UfoFilterSinoGenerator *self)
{
    UfoFilterSinoGeneratorPrivate *priv = self->priv = UFO_FILTER_SINO_GENERATOR_GET_PRIVATE(self);
    priv->num_projections = 1;
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_SINO_GENERATOR, NULL);
}
