#include <glib.h>

#include "ufo-connection.h"

G_DEFINE_TYPE(UfoConnection, ufo_connection, G_TYPE_OBJECT);

#define UFO_CONNECTION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CONNECTION, UfoConnectionPrivate))

struct _UfoConnectionPrivate {
    UfoElement   *source;
    UfoElement   *destination;
    GAsyncQueue *queue;
};

UfoConnection *ufo_connection_new()
{
    return g_object_new(UFO_TYPE_CONNECTION, NULL);
}

GAsyncQueue *ufo_connection_get_queue(UfoConnection *self)
{
    return self->priv->queue;
}

UfoElement *ufo_connection_get_destination(UfoConnection *self)
{
    return self->priv->destination;
}

void ufo_connection_set_elements(UfoConnection *self, UfoElement *src, UfoElement *dst)
{
    self->priv->source = src;
    self->priv->destination = dst;
}

static void ufo_connection_dispose(GObject *gobject)
{
    UfoConnection *self = UFO_CONNECTION(gobject);

    g_async_queue_unref(self->priv->queue);

    G_OBJECT_CLASS(ufo_connection_parent_class)->dispose(gobject);
}

static void ufo_connection_class_init(UfoConnectionClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose = ufo_connection_dispose;

    /* install private data */
    g_type_class_add_private(klass, sizeof(UfoConnectionPrivate));
}

static void ufo_connection_init(UfoConnection *self)
{
    /* init public fields */

    /* init private fields */
    self->priv = UFO_CONNECTION_GET_PRIVATE(self);
    self->priv->source = NULL;
    self->priv->destination = NULL;
    self->priv->queue = g_async_queue_new();
}


