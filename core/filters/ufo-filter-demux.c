#include <gmodule.h>
#include <CL/cl.h>

#include "ufo-resource-manager.h"
#include "ufo-filter-demux.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"

struct _UfoFilterDemuxPrivate {
};

GType ufo_filter_demux_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(UfoFilterDemux, ufo_filter_demux, UFO_TYPE_FILTER);

#define UFO_FILTER_DEMUX_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_DEMUX, UfoFilterDemuxPrivate))

enum {
    PROP_0,
    N_PROPERTIES
};

/*static GParamSpec *demux_properties[N_PROPERTIES] = { NULL, };*/

static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

/* 
 * virtual methods 
 */
static void ufo_filter_demux_initialize(UfoFilter *filter)
{
}

/*
 * This is the main method in which the filter processes one buffer after
 * another.
 */
static void ufo_filter_demux_process(UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    GAsyncQueue *input_queue = ufo_element_get_input_queue(UFO_ELEMENT(filter));

    GAsyncQueue *output_queues[2] = { NULL, NULL };
    output_queues[0] = ufo_filter_get_output_queue_by_name(filter, "output1");
    output_queues[1] = ufo_filter_get_output_queue_by_name(filter, "output2");
    int current = 0;

    UfoBuffer *input = (UfoBuffer *) g_async_queue_pop(input_queue);
    while (!ufo_buffer_is_finished(input)) {
        g_async_queue_push(output_queues[current], input);
        current = 1 - current;
        g_message("pushing to %i", current);
        input = (UfoBuffer *) g_async_queue_pop(input_queue);
    }

    /* input is finish buffer but is only transfered to the first output */
    g_async_queue_push(output_queues[0], input);
    g_async_queue_push(output_queues[1], ufo_resource_manager_request_finish_buffer(ufo_resource_manager()));
}

static void ufo_filter_demux_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    /* Handle all properties accordingly */
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_demux_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    /* Handle all properties accordingly */
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_demux_class_init(UfoFilterDemuxClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

    gobject_class->set_property = ufo_filter_demux_set_property;
    gobject_class->get_property = ufo_filter_demux_get_property;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;
    filter_class->initialize = ufo_filter_demux_initialize;
    filter_class->process = ufo_filter_demux_process;

    /* install private data */
    /*g_type_class_add_private(gobject_class, sizeof(UfoFilterDemuxPrivate));*/
}

static void ufo_filter_demux_init(UfoFilterDemux *self)
{
    static const gchar* input_names[] = { "input", NULL };
    UFO_FILTER_GET_CLASS(self)->install_inputs(UFO_FILTER(self), input_names);
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_DEMUX, NULL);
}
