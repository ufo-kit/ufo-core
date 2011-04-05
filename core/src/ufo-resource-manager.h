#ifndef __UFO_RESOURCE_MANAGER_H
#define __UFO_RESOURCE_MANAGER_H

#include <glib.h>
#include <glib-object.h>

#define UFO_TYPE_RESOURCE_MANAGER             (ufo_resource_manager_get_type())
#define UFO_RESOURCE_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_RESOURCE_MANAGER, UfoResourceManager))
#define UFO_IS_RESOURCE_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_RESOURCE_MANAGER))
#define UFO_RESOURCE_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_RESOURCE_MANAGER, UfoResourceManagerClass))
#define UFO_IS_RESOURCE_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_RESOURCE_MANAGER))
#define UFO_RESOURCE_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_RESOURCE_MANAGER, UfoResourceManagerClass))

typedef struct _UfoResourceManager           UfoResourceManager;
typedef struct _UfoResourceManagerClass      UfoResourceManagerClass;
typedef struct _UfoResourceManagerPrivate    UfoResourceManagerPrivate;


struct _UfoResourceManager {
    GObject parent_instance;

    /* public */

    /* private */
    UfoResourceManagerPrivate *priv;
};

struct _UfoResourceManagerClass {
    GObjectClass parent_class;

    /* class members */

    /* virtual public methods */
};

/* virtual public methods */

/* non-virtual public methods */

UfoResourceManager *ufo_resource_manager_new();

GType ufo_resource_manager_get_type(void);

#endif
