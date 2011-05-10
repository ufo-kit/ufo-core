#include <gmodule.h>
#include <stdio.h>

#include "ufo-filter-raw.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"
#include "ufo-resource-manager.h"


struct _UfoFilterRawPrivate {
    guint current_frame;
    gchar *prefix;
};

GType ufo_filter_raw_get_type(void) G_GNUC_CONST;

/* Inherit from UFO_TYPE_FILTER */
G_DEFINE_TYPE(UfoFilterRaw, ufo_filter_raw, UFO_TYPE_FILTER);

#define UFO_FILTER_RAW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_RAW, UfoFilterRawPrivate))

enum {
    PROP_0,
    PROP_PREFIX,
    N_PROPERTIES
};

static GParamSpec *raw_properties[N_PROPERTIES] = { NULL, };

/* 
 * virtual methods 
 */
static void activated(EthosPlugin *plugin)
{
    UfoFilterRawPrivate *priv = UFO_FILTER_RAW_GET_PRIVATE(plugin);
    priv->current_frame = 0;
}

static void deactivated(EthosPlugin *plugin)
{
}

static void ufo_filter_raw_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterRaw *filter = UFO_FILTER_RAW(object);

    switch (property_id) {
        case PROP_PREFIX:
            g_free(filter->priv->prefix);
            filter->priv->prefix = g_strdup(g_value_get_string(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_raw_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoFilterRaw *filter = UFO_FILTER_RAW(object);

    switch (property_id) {
        case PROP_PREFIX:
            g_value_set_string(value, filter->priv->prefix);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_raw_dispose(GObject *object)
{
    G_OBJECT_CLASS(ufo_filter_raw_parent_class)->dispose(object);
}

static void ufo_filter_raw_finalize(GObject *object)
{
    UfoFilterRaw *filter = UFO_FILTER_RAW(object);
    UfoFilterRawPrivate *priv = UFO_FILTER_RAW_GET_PRIVATE(filter);
    g_free(priv->prefix);
    G_OBJECT_CLASS(ufo_filter_raw_parent_class)->finalize(object);
}

static void ufo_filter_raw_process(UfoFilter *self)
{
    /* FIXME: filter should popping from the queue until finishing buffer comes by */
    g_return_if_fail(UFO_IS_FILTER(self));
    UfoFilterRawPrivate *priv = UFO_FILTER_RAW_GET_PRIVATE(self);
    GAsyncQueue *input_queue = ufo_element_get_input_queue(UFO_ELEMENT(self));
    GString *filename = g_string_new("");

    while (1) {
        g_message("[raw-%s] waiting on queue %p...", priv->prefix, input_queue);
        UfoBuffer *input = UFO_BUFFER(g_async_queue_pop(input_queue));
        g_message("[raw-%s] received buffer %p at queue %p", priv->prefix,
                input, ufo_element_get_input_queue(UFO_ELEMENT(self)));
        UfoResourceManager *manager = ufo_resource_manager();

        if (ufo_buffer_is_finished(input)) {
            ufo_resource_manager_release_buffer(manager, input);
            break;
        }

        gint32 width, height;
        ufo_buffer_get_dimensions(input, &width, &height);

        g_string_printf(filename, "%s-%ix%i-%i.raw", priv->prefix, width, height, priv->current_frame++);
        FILE *fp = fopen(filename->str, "wb");
        float *data = ufo_buffer_get_cpu_data(input);
        fwrite(data, sizeof(float), width*height, fp);
        fclose(fp);

        ufo_resource_manager_release_buffer(manager, input);
    }
    g_message("[raw-%s] done", priv->prefix);
}

static void ufo_filter_raw_class_init(UfoFilterRawClass *klass)
{
    /* override methods */
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose = ufo_filter_raw_dispose;
    gobject_class->finalize = ufo_filter_raw_finalize;
    gobject_class->get_property = ufo_filter_raw_get_property;
    gobject_class->set_property = ufo_filter_raw_set_property;

    filter_class->process = ufo_filter_raw_process;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;

    /* install properties */
    raw_properties[PROP_PREFIX] = 
        g_param_spec_string("prefix",
            "Filename prefix",
            "Prefix of output filename",
            "out",
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_PREFIX, raw_properties[PROP_PREFIX]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterRawPrivate));
}

static void ufo_filter_raw_init(UfoFilterRaw *self)
{
    /* init public fields */

    /* init private fields */
    self->priv = UFO_FILTER_RAW_GET_PRIVATE(self);
    self->priv->current_frame = 0;
    self->priv->prefix = g_strdup("prefix");
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_RAW, NULL);
}
