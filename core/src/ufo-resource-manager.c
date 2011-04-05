#include <glib.h>
#include "ufo-resource-manager.h"

G_DEFINE_TYPE(UfoResourceManager, ufo_resource_manager, G_TYPE_OBJECT);

#define UFO_RESOURCE_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_RESOURCE_MANAGER, UfoResourceManagerPrivate))

struct _UfoResourceManagerPrivate {
};

UfoResourceManager *ufo_resource_manager_new()
{
    return g_object_new(UFO_TYPE_RESOURCE_MANAGER, NULL);
}

static void ufo_resource_manager_dispose(GObject *gobject)
{
    /*UfoResourceManager *self = UFO_RESOURCE_MANAGER(gobject);*/

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
}


