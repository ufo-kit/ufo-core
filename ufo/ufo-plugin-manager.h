/*
 * Copyright (C) 2011-2013 Karlsruhe Institute of Technology
 *
 * This file is part of Ufo.
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __UFO_PLUGIN_MANAGER_H
#define __UFO_PLUGIN_MANAGER_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <ufo/ufo-task-node.h>

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

UfoPluginManager  * ufo_plugin_manager_new                  (void);
GObject           * ufo_plugin_manager_get_plugin           (UfoPluginManager   *manager,
                                                             const gchar        *func_name,
                                                             const gchar        *module_name,
                                                             GError            **error);
GList             * ufo_plugin_get_all_plugin_names         (UfoPluginManager   *manager,
                                                             const GRegex       *filename_regex,
                                                             const gchar        *filename_pattern);
UfoTaskNode       * ufo_plugin_manager_get_task             (UfoPluginManager   *manager,
                                                             const gchar        *name,
                                                             GError            **error);
UfoTaskNode       * ufo_plugin_manager_get_task_from_package(UfoPluginManager   *manager,
                                                             const gchar        *package_name,
                                                             const gchar        *name,
                                                             GError            **error);
GList             * ufo_plugin_manager_get_all_task_names   (UfoPluginManager   *manager);
GType               ufo_plugin_manager_get_type             (void);

G_END_DECLS

#endif
