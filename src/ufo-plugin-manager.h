#ifndef __UFO_PLUGIN_MANAGER_H
#define __UFO_PLUGIN_MANAGER_H

#include <glib-object.h>
#include "ufo-filter.h"

G_BEGIN_DECLS

#define UFO_TYPE_PLUGIN_MANAGER             (ufo_plugin_manager_get_type())
#define UFO_PLUGIN_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_PLUGIN_MANAGER, UfoPluginManager))
#define UFO_IS_PLUGIN_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_PLUGIN_MANAGER))
#define UFO_PLUGIN_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_PLUGIN_MANAGER, UfoPluginManagerClass))
#define UFO_IS_PLUGIN_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_PLUGIN_MANAGER))
#define UFO_PLUGIN_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_PLUGIN_MANAGER, UfoPluginManagerClass))

typedef struct _UfoPluginManager           UfoPluginManager;
typedef struct _UfoPluginManagerClass      UfoPluginManagerClass;
typedef struct _UfoPluginManagerPrivate    UfoPluginManagerPrivate;

struct _UfoPluginManager {
    GObject parent_instance;

    /* private */
    UfoPluginManagerPrivate *priv;
};

struct _UfoPluginManagerClass {
    GObjectClass parent_class;
};

UfoPluginManager *ufo_plugin_manager_new(void);
void ufo_plugin_manager_add_paths(UfoPluginManager *manager, const gchar *paths);
UfoFilter *ufo_plugin_manager_get_filter(UfoPluginManager *manager, const gchar *name);

GType ufo_plugin_manager_get_type(void);

G_END_DECLS

#endif
