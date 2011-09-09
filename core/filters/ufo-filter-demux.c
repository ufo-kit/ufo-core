#include <gmodule.h>
#include <CL/cl.h>

#include "ufo-resource-manager.h"
#include "ufo-filter-demux.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"

struct _UfoFilterDemuxPrivate {
    gboolean copy;
};

GType ufo_filter_demux_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(UfoFilterDemux, ufo_filter_demux, UFO_TYPE_FILTER);

#define UFO_FILTER_DEMUX_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_DEMUX, UfoFilterDemuxPrivate))

enum {
    PROP_0,
    PROP_COPY,
    N_PROPERTIES
};

static GParamSpec *demux_properties[N_PROPERTIES] = { NULL, };

static void filter_demux_process_simple(UfoFilterDemux *self)
{
    GAsyncQueue *input_queue = ufo_element_get_input_queue(UFO_ELEMENT(self));

    GAsyncQueue *output_queues[2] = { NULL, NULL };
    output_queues[0] = ufo_filter_get_output_queue_by_name(UFO_FILTER(self), "output1");
    output_queues[1] = ufo_filter_get_output_queue_by_name(UFO_FILTER(self), "output2");
    int current = 0;

    UfoBuffer *input = (UfoBuffer *) g_async_queue_pop(input_queue);
    while (!ufo_buffer_is_finished(input)) {
        g_async_queue_push(output_queues[current], input);
        current = 1 - current;
        input = (UfoBuffer *) g_async_queue_pop(input_queue);
    }

    /* input is finish buffer but is only transfered to the first output */
    g_async_queue_push(output_queues[0], input);
    g_async_queue_push(output_queues[1], ufo_resource_manager_request_finish_buffer(ufo_resource_manager()));
}

static void filter_demux_process_copy(UfoFilterDemux *self)
{
    UfoResourceManager *manager = ufo_resource_manager();
    GAsyncQueue *input_queue = ufo_element_get_input_queue(UFO_ELEMENT(self));

    GAsyncQueue *output_queues[2] = { NULL, NULL };
    output_queues[0] = ufo_filter_get_output_queue_by_name(UFO_FILTER(self), "output1");
    output_queues[1] = ufo_filter_get_output_queue_by_name(UFO_FILTER(self), "output2");
    
    UfoBuffer *input = (UfoBuffer *) g_async_queue_pop(input_queue);
    while (!ufo_buffer_is_finished(input)) {
        /* Assign the current input a new id, so that it is ordered _after_ the
         * last copy buffer */
        ufo_buffer_increment_id(input);

        UfoBuffer *copy = ufo_resource_manager_copy_buffer(manager, input);
        g_async_queue_push(output_queues[0], input);
        g_async_queue_push(output_queues[1], copy);
        input = (UfoBuffer *) g_async_queue_pop(input_queue);
    }

    /* input is finish buffer but is only transfered to the first output */
    g_async_queue_push(output_queues[0], input);
    g_async_queue_push(output_queues[1], ufo_resource_manager_request_finish_buffer(manager));
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
    UfoFilterDemux *self = UFO_FILTER_DEMUX(filter);
    if (self->priv->copy)
        filter_demux_process_copy(self); 
    else
        filter_demux_process_simple(self);
}

static void ufo_filter_demux_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterDemux *self = UFO_FILTER_DEMUX(object);
    switch (property_id) {
        case PROP_COPY:
            self->priv->copy = g_value_get_boolean(value);
            break;
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
    UfoFilterDemux *self = UFO_FILTER_DEMUX(object);
    switch (property_id) {
        case PROP_COPY:
            g_value_set_boolean(value, self->priv->copy);
            break;
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
    
    /* install properties */
    demux_properties[PROP_COPY] = 
        g_param_spec_boolean("copy",
            "Copy input to both queues, incrementing the ID of the buffer for the second output",
            "Copy input to both queues, incrementing the ID of the buffer for the second output",
            FALSE,
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_COPY, demux_properties[PROP_COPY]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterDemuxPrivate));
}

static void ufo_filter_demux_init(UfoFilterDemux *self)
{
    static const gchar* input_names[] = { "input", NULL };
    UFO_FILTER_GET_CLASS(self)->install_inputs(UFO_FILTER(self), input_names);
    
    UfoFilterDemuxPrivate *priv = UFO_FILTER_DEMUX_GET_PRIVATE(self);
    self->priv = priv;
    priv->copy = FALSE;
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_DEMUX, NULL);
}
