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
    UfoChannel *input_channel = ufo_filter_get_input_channel(UFO_FILTER(self));
    UfoChannel *output_channels[2] = { NULL, NULL };
    output_channels[0] = ufo_filter_get_output_channel_by_name(UFO_FILTER(self), "output1");
    output_channels[1] = ufo_filter_get_output_channel_by_name(UFO_FILTER(self), "output2");
    int current = 0;

    UfoBuffer *input = ufo_channel_pop(input_channel);
    while (input != NULL) {
        ufo_channel_push(output_channels[current], input);
        current = 1 - current;
        input = ufo_channel_pop(input_channel);
    }

    ufo_channel_finish(output_channels[0]);
    ufo_channel_finish(output_channels[1]);
}

static void filter_demux_process_copy_same(UfoFilterDemux *self)
{
    UfoResourceManager *manager = ufo_resource_manager();
    UfoChannel *input_channel = ufo_filter_get_input_channel(UFO_FILTER(self));
    UfoChannel *output_channels[2] = { NULL, NULL };
    output_channels[0] = ufo_filter_get_output_channel_by_name(UFO_FILTER(self), "output1");
    output_channels[1] = ufo_filter_get_output_channel_by_name(UFO_FILTER(self), "output2");
    
    UfoBuffer *input = ufo_channel_pop(input_channel);
    while (input != NULL) {
        /* Assign the current input a new id, so that it is ordered _after_ the
         * last copy buffer */
        ufo_buffer_increment_id(input);

        UfoBuffer *copy = ufo_resource_manager_copy_buffer(manager, input);
        ufo_channel_push(output_channels[0], input);
        ufo_channel_push(output_channels[1], copy);
        input = ufo_channel_pop(input_channel);
    }

    ufo_channel_finish(output_channels[0]);
    ufo_channel_finish(output_channels[1]);
}

static void filter_demux_process_copy_delayed(UfoFilterDemux *self)
{
    UfoResourceManager *manager = ufo_resource_manager();
    UfoChannel *input_channel = ufo_filter_get_input_channel(UFO_FILTER(self));
    UfoChannel *output_channels[2] = { NULL, NULL };
    output_channels[0] = ufo_filter_get_output_channel_by_name(UFO_FILTER(self), "output1");
    output_channels[1] = ufo_filter_get_output_channel_by_name(UFO_FILTER(self), "output2");
    
    UfoBuffer *input1 = ufo_channel_pop(input_channel);
    while (input1 != NULL) {
        ufo_channel_push(output_channels[0], input1);
        UfoBuffer *input2 = ufo_channel_pop(input_channel);
        if (input2 != NULL) {
            input1 = ufo_resource_manager_copy_buffer(manager, input2);
            ufo_channel_push(output_channels[1], input2);
        }
        else
            input1 = NULL;
    }

    ufo_channel_finish(output_channels[0]);
    ufo_channel_finish(output_channels[1]);
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
