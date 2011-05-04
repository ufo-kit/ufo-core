#include <gmodule.h>

#include "ufo-filter-reader.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"
#include "ufo-resource-manager.h"


struct _UfoFilterReaderPrivate {
    int dummy;
};

GType ufo_filter_reader_get_type(void) G_GNUC_CONST;

/* Inherit from UFO_TYPE_FILTER */
G_DEFINE_TYPE(UfoFilterReader, ufo_filter_reader, UFO_TYPE_FILTER);

#define UFO_FILTER_READER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_READER, UfoFilterReaderPrivate))


/* 
 * virtual methods 
 */
static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

static void ufo_filter_reader_dispose(GObject *object)
{
    G_OBJECT_CLASS(ufo_filter_reader_parent_class)->dispose(object);
}

static void ufo_filter_reader_process(UfoFilter *self)
{
    g_return_if_fail(UFO_IS_FILTER(self));

    /*UfoFilterReaderPrivate *priv = UFO_FILTER_READER_GET_PRIVATE(self);*/

    UfoResourceManager *manager = ufo_filter_get_resource_manager(self);
    GAsyncQueue *output_queue = ufo_element_get_output_queue(UFO_ELEMENT(self));

    /* interpret prefix data and spit out images from disk */

    /* No more data */
    UfoBuffer *buffer = ufo_resource_manager_request_buffer(manager, 1, 1, NULL);
    g_object_set(buffer,
            "finished", TRUE,
            NULL);
    g_async_queue_push(output_queue, buffer);
}

static void ufo_filter_reader_class_init(UfoFilterReaderClass *klass)
{
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = ufo_filter_reader_dispose;
    filter_class->process = ufo_filter_reader_process;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;

    /* install private data */
    g_type_class_add_private(object_class, sizeof(UfoFilterReaderPrivate));
}

static void ufo_filter_reader_init(UfoFilterReader *self)
{
    self->priv = UFO_FILTER_READER_GET_PRIVATE(self);
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_READER, NULL);
}
