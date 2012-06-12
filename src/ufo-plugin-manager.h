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

#define UFO_PLUGIN_MANAGER_ERROR ufo_plugin_manager_error_quark()
GQuark ufo_plugin_manager_error_quark(void);

typedef enum {
    UFO_PLUGIN_MANAGER_ERROR_MODULE_NOT_FOUND,
    UFO_PLUGIN_MANAGER_ERROR_MODULE_OPEN,
    UFO_PLUGIN_MANAGER_ERROR_SYMBOL_NOT_FOUND
} UfoPluginManagerError;

typedef struct _UfoPluginManager           UfoPluginManager;
typedef struct _UfoPluginManagerClass      UfoPluginManagerClass;
typedef struct _UfoPluginManagerPrivate    UfoPluginManagerPrivate;

/**
 * UfoPluginManager:
 *
 * Creates #UfoFilter instances by loading corresponding shared objects. The
 * contents of the #UfoPluginManager structure are private and should only be
 * accessed via the provided API.
 */
struct _UfoPluginManager {
    /*< private >*/
    GObject parent_instance;

    UfoPluginManagerPrivate *priv;
};

/**
 * UfoPluginManagerClass:
 *
 * #UfoPluginManager class
 */
struct _UfoPluginManagerClass {
    /*< private >*/
    GObjectClass parent_class;
};

UfoPluginManager   *ufo_plugin_manager_new                  (const gchar        *paths);
UfoFilter          *ufo_plugin_manager_get_filter           (UfoPluginManager   *manager, 
                                                             const gchar        *name, 
                                                             GError            **error);
GList              *ufo_plugin_manager_available_filters    (UfoPluginManager   *manager);
GType               ufo_plugin_manager_get_type             (void);

G_END_DECLS

#endif
