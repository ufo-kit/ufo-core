#include <gmodule.h>
#include <CL/cl.h>

#include "ufo-resource-manager.h"
#include "ufo-filter-mux.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"

struct _UfoFilterMuxPrivate {
};

GType ufo_filter_mux_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(UfoFilterMux, ufo_filter_mux, UFO_TYPE_FILTER);

#define UFO_FILTER_MUX_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_MUX, UfoFilterMuxPrivate))

enum {
    PROP_0,
    N_PROPERTIES
};

/* static GParamSpec *mux_properties[N_PROPERTIES] = { NULL, }; */

static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

/* 
 * virtual methods 
 */
static void ufo_filter_mux_initialize(UfoFilter *filter)
{
}

/*
 * This is the main method in which the filter processes one buffer after
 * another.
 */
static void ufo_filter_mux_process(UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    GAsyncQueue *input_queues[2] = { NULL, NULL };
    input_queues[0] = ufo_filter_get_input_queue_by_name(filter, "input1");
    input_queues[1] = ufo_filter_get_input_queue_by_name(filter, "input2");
    GAsyncQueue *output_queue = ufo_element_get_output_queue(UFO_ELEMENT(filter));

    UfoBuffer *input1 = (UfoBuffer *) g_async_queue_pop(input_queues[0]);
    UfoBuffer *input2 = (UfoBuffer *) g_async_queue_pop(input_queues[1]);
    gint id1 = ufo_buffer_get_id(input1);
    gint id2 = ufo_buffer_get_id(input2);
    
    while (!ufo_buffer_is_finished(input1) || !ufo_buffer_is_finished(input2)) {
        while ((id1 < id2) && !ufo_buffer_is_finished(input1)) {
            g_async_queue_push(output_queue, input1);
            input1 = g_async_queue_pop(input_queues[0]);
            id1 = ufo_buffer_get_id(input1);
        }
        
        while ((id2 < id1) && !ufo_buffer_is_finished(input2)) {
            g_async_queue_push(output_queue, input2);
            input2 = g_async_queue_pop(input_queues[1]);
            id2 = ufo_buffer_get_id(input2);
        }

        if (!ufo_buffer_is_finished(input1)) {
            g_async_queue_push(output_queue, input1);
            input1 = g_async_queue_pop(input_queues[0]);
            id1 = ufo_buffer_get_id(input1);
        }
        
        if (!ufo_buffer_is_finished(input2)) {
            g_async_queue_push(output_queue, input2);
            input2 = g_async_queue_pop(input_queues[1]);
            id2 = ufo_buffer_get_id(input2);
        }
    }
    
    /* Discard one of the finishing buffers and push the other */
    ufo_resource_manager_release_buffer(ufo_resource_manager(), input1);
    g_async_queue_push(output_queue, input2);
}

static void ufo_filter_mux_set_property(GObject *object,
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

static void ufo_filter_mux_get_property(GObject *object,
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

static void ufo_filter_mux_class_init(UfoFilterMuxClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

    gobject_class->set_property = ufo_filter_mux_set_property;
    gobject_class->get_property = ufo_filter_mux_get_property;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;
    filter_class->initialize = ufo_filter_mux_initialize;
    filter_class->process = ufo_filter_mux_process;
}

static void ufo_filter_mux_init(UfoFilterMux *self)
{
    static const gchar* input_names[] = { "input1", "input2", NULL };
    UFO_FILTER_GET_CLASS(self)->install_inputs(UFO_FILTER(self), input_names);
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_MUX, NULL);
}
