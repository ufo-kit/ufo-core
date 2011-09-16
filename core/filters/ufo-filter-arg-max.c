#include <gmodule.h>
#include <CL/cl.h>

#include "ufo-resource-manager.h"
#include "ufo-filter-arg-max.h"
#include "ufo-filter.h"
#include "ufo-buffer.h"

GType ufo_filter_arg_max_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(UfoFilterArgMax, ufo_filter_arg_max, UFO_TYPE_FILTER);

#define UFO_FILTER_ARG_MAX_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_ARG_MAX, UfoFilterArgMaxPrivate))

enum {
    PROP_0,
    N_PROPERTIES
};

/* static GParamSpec *arg_max_properties[N_PROPERTIES] = { NULL, }; */

static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

/* 
 * virtual methods 
 */
static void ufo_filter_arg_max_initialize(UfoFilter *filter)
{
}

/*
 * This is the main method in which the filter processes one buffer after
 * another.
 */
static void ufo_filter_arg_max_process(UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    UfoChannel *input_channel = ufo_filter_get_input_channel(filter);
    UfoChannel *output_channel = ufo_filter_get_output_channel(filter);
    UfoBuffer *input = ufo_channel_pop(input_channel);
    
    cl_command_queue command_queue = (cl_command_queue) ufo_filter_get_command_queue(filter);
    gint index[4] = {0, 0, 0, 0};
    gint dims[4] = {1, 1, 1, 1};

    while (input != NULL) {
        index[0] = index[1] = index[2] = index[3] = 0;
        ufo_buffer_get_dimensions(input, dims);
        float *data = ufo_buffer_get_cpu_data(input, command_queue);
        float max_value = data[0];
        
        int y_offset = dims[0];
        int z_offset = dims[0]*dims[1];
        int t_offset = z_offset*dims[2];
        
        for (int x = 0; x < dims[0]; x++) {
            for (int y = 0; y < dims[1]; y++) {
                for (int z = 0; z < dims[2]; z++) {
                    for (int t = 0; t < dims[3]; t++) {
                        float v = data[x + y*y_offset + z*z_offset + t*t_offset];
                        if (v > max_value) {
                            max_value = v;
                            index[0] = x; index[1] = y; index[2] = z; index[3] = t;
                        }
                    }
                }
            }
        }
        g_message("Maximum at <%i,%i,%i,%i>", index[0], index[1], index[2], index[3]);

        ufo_channel_push(output_channel, input);
        input = ufo_channel_pop(input_channel);
    }
    ufo_channel_finish(output_channel);
}

static void ufo_filter_arg_max_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterArgMax *self = UFO_FILTER_ARG_MAX(object);

    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_arg_max_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoFilterArgMax *self = UFO_FILTER_ARG_MAX(object);

    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_arg_max_class_init(UfoFilterArgMaxClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

    gobject_class->set_property = ufo_filter_arg_max_set_property;
    gobject_class->get_property = ufo_filter_arg_max_get_property;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;
    filter_class->initialize = ufo_filter_arg_max_initialize;
    filter_class->process = ufo_filter_arg_max_process;

}

static void ufo_filter_arg_max_init(UfoFilterArgMax *self)
{
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_ARG_MAX, NULL);
}
