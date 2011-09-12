#include <gmodule.h>
#include <CL/cl.h>

#include "ufo-resource-manager.h"
#include "ufo-filter-demux.h"
#include "ufo-filter.h"
#include "ufo-buffer.h"

typedef enum {
    COPY_NA,
    COPY_SAME,
    COPY_DELAYED
} CopyMode;

struct _UfoFilterDemuxPrivate {
    CopyMode mode; 
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
    GAsyncQueue *input_queue = ufo_filter_get_input_queue(UFO_FILTER(self));

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

static void filter_demux_process_copy_same(UfoFilterDemux *self)
{
    UfoResourceManager *manager = ufo_resource_manager();
    GAsyncQueue *input_queue = ufo_filter_get_input_queue(UFO_FILTER(self));

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

static void filter_demux_process_copy_delayed(UfoFilterDemux *self)
{
    UfoResourceManager *manager = ufo_resource_manager();
    GAsyncQueue *input_queue = ufo_filter_get_input_queue(UFO_FILTER(self));

    GAsyncQueue *output_queues[2] = { NULL, NULL };
    output_queues[0] = ufo_filter_get_output_queue_by_name(UFO_FILTER(self), "output1");
    output_queues[1] = ufo_filter_get_output_queue_by_name(UFO_FILTER(self), "output2");
    
    UfoBuffer *input1 = (UfoBuffer *) g_async_queue_pop(input_queue);
    while (!ufo_buffer_is_finished(input1)) {
        g_async_queue_push(output_queues[0], input1);
        UfoBuffer *input2 = (UfoBuffer *) g_async_queue_pop(input_queue);
        input1 = ufo_resource_manager_copy_buffer(manager, input2);
        g_async_queue_push(output_queues[1], input2);
    }

    /* input is finish buffer but is only transfered to the first output */
    g_async_queue_push(output_queues[0], input1);
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

static void ufo_filter_demux_process(UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    UfoFilterDemux *self = UFO_FILTER_DEMUX(filter);
    switch (self->priv->mode) {
        case COPY_NA: 
            filter_demux_process_simple(self);
            break;
        case COPY_SAME:
            filter_demux_process_copy_same(self); 
            break;
        case COPY_DELAYED:
            filter_demux_process_copy_delayed(self);
            break;
        default:
            g_warning("Copy mode unknown");
    }
}

static void ufo_filter_demux_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterDemux *self = UFO_FILTER_DEMUX(object);
    switch (property_id) {
        case PROP_COPY:
            if (!g_strcmp0(g_value_get_string(value), "same"))
                self->priv->mode = COPY_SAME;
            else if (!g_strcmp0(g_value_get_string(value), "delayed"))
                self->priv->mode = COPY_DELAYED;
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
            switch (self->priv->mode) {
                case COPY_SAME: 
                    g_value_set_string(value, "same");
                    break;
                case COPY_DELAYED:
                    g_value_set_string(value, "delayed");
                    break;
                default:
                    g_value_set_string(value, "na");
            }
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
        g_param_spec_string("copy",
            "Copy mode can be \"same\" or \"delayed\"",
            "Copy mode can be \"same\" or \"delayed\"",
            "na",
            G_PARAM_READWRITE);
    
    g_object_class_install_property(gobject_class, PROP_COPY, demux_properties[PROP_COPY]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterDemuxPrivate));
}

static void ufo_filter_demux_init(UfoFilterDemux *self)
{
    UfoFilterDemuxPrivate *priv = UFO_FILTER_DEMUX_GET_PRIVATE(self);
    self->priv = priv;
    priv->mode = COPY_NA;
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_DEMUX, NULL);
}
