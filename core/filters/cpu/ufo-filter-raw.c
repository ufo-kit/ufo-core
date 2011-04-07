#include <gmodule.h>
#include <stdio.h>

#include "ufo-filter-raw.h"
#include "ufo-filter.h"
#include "ufo-buffer.h"
#include "ufo-resource-manager.h"


struct _UfoFilterRawPrivate {
    guint current_frame;
};

GType ufo_filter_raw_get_type(void) G_GNUC_CONST;

/* Inherit from UFO_TYPE_FILTER */
G_DEFINE_TYPE(UfoFilterRaw, ufo_filter_raw, UFO_TYPE_FILTER);

#define UFO_FILTER_RAW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_RAW, UfoFilterRawPrivate))


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

static void ufo_filter_raw_dispose(GObject *object)
{
    G_OBJECT_CLASS(ufo_filter_raw_parent_class)->dispose(object);
}

static void ufo_filter_raw_process(UfoFilter *self)
{
    g_return_if_fail(UFO_IS_FILTER(self));

    UfoBuffer *input = (UfoBuffer *) g_async_queue_pop(ufo_filter_get_input_queue(self));
    gint32 width, height;
    ufo_buffer_get_dimensions(input, &width, &height);

    UfoFilterRawPrivate *priv = UFO_FILTER_RAW_GET_PRIVATE(self);

    GString *filename = g_string_new("");
    g_string_printf(filename, "prefix-%ix%i-%i.raw", width, height, priv->current_frame);
    FILE *fp = fopen(filename->str, "wb");
    fwrite(ufo_buffer_get_cpu_data(input), sizeof(float), width*height, fp);
    fclose(fp);

    priv->current_frame++;

    UfoResourceManager *manager = ufo_filter_get_resource_manager(self);
    ufo_resource_manager_release_buffer(manager, input);
}

static void ufo_filter_raw_class_init(UfoFilterRawClass *klass)
{
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = ufo_filter_raw_dispose;
    filter_class->process = ufo_filter_raw_process;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;

    /* install private data */
    g_type_class_add_private(object_class, sizeof(UfoFilterRawPrivate));
}

static void ufo_filter_raw_init(UfoFilterRaw *self)
{
    /* init public fields */

    /* init private fields */
    self->priv = UFO_FILTER_RAW_GET_PRIVATE(self);
    self->priv->current_frame = 0;
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_RAW, NULL);
}
