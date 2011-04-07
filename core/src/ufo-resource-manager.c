#include <glib.h>
#include <CL/cl.h>

#include "ufo-resource-manager.h"

G_DEFINE_TYPE(UfoResourceManager, ufo_resource_manager, G_TYPE_OBJECT);

#define UFO_RESOURCE_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_RESOURCE_MANAGER, UfoResourceManagerPrivate))

struct _UfoResourceManagerPrivate {
    GHashTable *buffers; /**< maps from dimension hash to a GTrashStack of buffer instances */
};

UfoResourceManager *ufo_resource_manager_new()
{
    return g_object_new(UFO_TYPE_RESOURCE_MANAGER, NULL);
}

static guint32 ufo_resource_manager_hash_dims(guint32 width, guint32 height)
{
    guint32 result = 0x345678;
    result ^= width << 12;
    result ^= height;
    return result;
}

UfoBuffer *ufo_resource_manager_request_buffer(UfoResourceManager *self, guint32 width, guint32 height)
{
    const gpointer hash = GINT_TO_POINTER(ufo_resource_manager_hash_dims(width, height));

    GQueue *queue = g_hash_table_lookup(self->priv->buffers, hash);
    UfoBuffer *buffer = NULL;

    if (queue == NULL) {
        /* If there is no queue for this hash we create a new one but don't fill
         * it with the newly created buffer */
        buffer = ufo_buffer_new(width, height);
        queue = g_queue_new();
        g_hash_table_insert(self->priv->buffers, hash, queue);
    }
    else {
        buffer = g_queue_pop_head(queue);
        if (buffer == NULL)
            buffer = ufo_buffer_new(width, height);
    }
    g_message("requesting buffer %p", buffer);
    
    return buffer;
}

void ufo_resource_manager_release_buffer(UfoResourceManager *self, UfoBuffer *buffer)
{
    gint32 width, height;
    ufo_buffer_get_dimensions(buffer, &width, &height);
    const gpointer hash = GINT_TO_POINTER(ufo_resource_manager_hash_dims(width, height));

    g_message("putting buffer %p back", buffer);

    GQueue *queue = g_hash_table_lookup(self->priv->buffers, hash);
    if (queue == NULL) { /* should not be the case */
        queue = g_queue_new();
        g_hash_table_insert(self->priv->buffers, hash, queue);
    }
    g_queue_push_head(queue, buffer); 
}


static void ufo_resource_manager_dispose(GObject *gobject)
{
    UfoResourceManager *self = UFO_RESOURCE_MANAGER(gobject);

    g_hash_table_destroy(self->priv->buffers);

    G_OBJECT_CLASS(ufo_resource_manager_parent_class)->dispose(gobject);
}

static void ufo_resource_manager_class_init(UfoResourceManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose = ufo_resource_manager_dispose;

    /* install private data */
    g_type_class_add_private(klass, sizeof(UfoResourceManagerPrivate));
}

static void ufo_resource_manager_init(UfoResourceManager *self)
{
    /* init public fields */

    /* init private fields */
    self->priv = UFO_RESOURCE_MANAGER_GET_PRIVATE(self);
    self->priv->buffers = g_hash_table_new(NULL, NULL);
}


