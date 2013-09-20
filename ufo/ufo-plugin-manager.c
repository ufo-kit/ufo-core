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

#include "config.h"
#include <gmodule.h>
#include <glob.h>
#include <ufo/ufo-plugin-manager.h>
#include <ufo/ufo-configurable.h>
#include <ufo/ufo-task-node.h>
#include <ufo/ufo-dummy-task.h>

/**
 * SECTION:ufo-plugin-manager
 * @Short_description: Load an #UfoFilter from a shared object
 * @Title: UfoPluginManager
 *
 * The plugin manager opens shared object modules searched for in locations
 * specified with ufo_plugin_manager_add_paths(). An #UfoFilter can be
 * instantiated with ufo_plugin_manager_get_task() with a one-to-one mapping
 * between filter name xyz and module name libufofilterxyz.so. Any errors are
 * reported as one of #UfoPluginManagerError codes.
 *
 * Apart from standard locations and paths passed through the #UfoConfig object,
 * #UfoPluginManager also looks into the path that is specified in the
 * UFO_PLUGIN_PATH environment variable.
 */

G_DEFINE_TYPE_WITH_CODE (UfoPluginManager, ufo_plugin_manager, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_CONFIGURABLE, NULL))

#define UFO_PLUGIN_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_PLUGIN_MANAGER, UfoPluginManagerPrivate))

typedef UfoNode* (* NewFunc) (void);

struct _UfoPluginManagerPrivate {
    GList       *search_paths;
    GSList      *modules;
    GHashTable  *new_funcs;  /**< maps from gchar* to NewFunc* */
};

enum {
    PROP_0,
    PROP_CONFIG,
    N_PROPERTIES
};

/**
 * UfoPluginManagerError:
 * @UFO_PLUGIN_MANAGER_ERROR_MODULE_NOT_FOUND: The module could not be found
 * @UFO_PLUGIN_MANAGER_ERROR_MODULE_OPEN: Module could not be opened
 * @UFO_PLUGIN_MANAGER_ERROR_SYMBOL_NOT_FOUND: Necessary entry symbol was not
 *      found
 *
 * Possible errors that ufo_plugin_manager_get_filter() can return.
 */
GQuark
ufo_plugin_manager_error_quark (void)
{
    return g_quark_from_static_string ("ufo-plugin-manager-error-quark");
}

static gchar *
plugin_manager_get_path (UfoPluginManagerPrivate *priv, const gchar *name)
{
    /* Check first if filename is already a path */
    if (g_path_is_absolute (name)) {
        if (g_file_test (name, G_FILE_TEST_EXISTS))
            return g_strdup (name);
        else
            return NULL;
    }

    /* If it is not a path, search in all known paths */
    for (GList *p = g_list_first (priv->search_paths); p != NULL; p = g_list_next (p)) {
        gchar *path = g_build_filename ((gchar *) p->data, name, NULL);

        if (g_file_test (path, G_FILE_TEST_EXISTS))
            return path;

        g_free (path);
    }

    return NULL;
}

static void
copy_config_paths (UfoPluginManagerPrivate *priv, UfoConfig *config)
{
    priv->search_paths = g_list_concat (ufo_config_get_paths (config), priv->search_paths);
}

/**
 * ufo_plugin_manager_new:
 * @config: (allow-none): A #UfoConfig object or %NULL.
 *
 * Create a plugin manager object to instantiate filter objects. When a config
 * object is passed to the constructor, its search-path property is added to the
 * internal search paths.
 *
 * Return value: A new plugin manager object.
 */
UfoPluginManager *
ufo_plugin_manager_new (UfoConfig *config)
{
    UfoPluginManager *manager;

    manager = UFO_PLUGIN_MANAGER (g_object_new (UFO_TYPE_PLUGIN_MANAGER,
                                                "config", config,
                                                NULL));

    return manager;
}


/**
 * ufo_plugin_manager_get_plugin:
 * @manager: A #UfoPluginManager
 * @func_name: Name of the constructor function.
 * @module_name: Filename of the shared object.
 * @error: return location for a GError or %NULL
 *
 * Load a module and return an instance.
 *
 * Returns: (transfer full): (allow-none): #gpointer or %NULL if module cannot be found
 */
gpointer
ufo_plugin_manager_get_plugin (UfoPluginManager *manager, 
                              const gchar *func_name,
                              const gchar *module_name,
                              GError **error)
{
    g_return_val_if_fail (UFO_IS_PLUGIN_MANAGER (manager) &&
                          func_name != NULL &&
                          module_name != NULL, NULL);

    UfoPluginManagerPrivate *priv = UFO_PLUGIN_MANAGER_GET_PRIVATE (manager);
    gpointer plugin = NULL;
    NewFunc *func = NULL;
    GModule *module = NULL;

    gchar *key = g_strjoin("@", func_name, module_name, NULL);
    func = g_hash_table_lookup (priv->new_funcs, key);

    if (func == NULL) {
        gchar *path = plugin_manager_get_path (priv, module_name);

        if (path == NULL) {
            g_set_error (error, UFO_PLUGIN_MANAGER_ERROR, UFO_PLUGIN_MANAGER_ERROR_MODULE_NOT_FOUND,
                         "Module %s not found", module_name);
            goto handle_error;
        }

        module = g_module_open (path, G_MODULE_BIND_LAZY);
        g_free (path);

        if (!module) {
            g_set_error (error, UFO_PLUGIN_MANAGER_ERROR, UFO_PLUGIN_MANAGER_ERROR_MODULE_OPEN,
                         "Module %s could not be opened: %s", module_name, g_module_error ());
            goto handle_error;
        }

        func = g_malloc0 (sizeof (NewFunc));

        if (!g_module_symbol (module, func_name, (gpointer *) func)) {
            g_set_error (error, UFO_PLUGIN_MANAGER_ERROR, UFO_PLUGIN_MANAGER_ERROR_SYMBOL_NOT_FOUND,
                         "%s is not exported by module %s: %s", func_name, module_name, g_module_error ());
            g_free (func);

            if (!g_module_close (module))
                g_warning ("%s", g_module_error ());

            goto handle_error;
        }

        priv->modules = g_slist_append (priv->modules, module);
        g_hash_table_insert (priv->new_funcs, g_strdup (key), func);
        g_free (key);
    }

    plugin = (*func) ();
    return plugin;

handle_error:
    g_free (key);
    return NULL;
}

/**
 * ufo_plugin_get_all_plugin_names:
 * @manager: A #UfoPluginManager
 * @filename_regex: Regex for filenames
 * @filename_pattern: Pattern according with the files will be searched
 *
 * Return a list with potential plugin names that match shared objects in all
 * search paths.
 *
 * Return value: (element-type utf8) (transfer full): List of strings with filter names
 */
GList*
ufo_plugin_get_all_plugin_names (UfoPluginManager *manager,
                                 const GRegex *filename_regex,
                                 const gchar *filename_pattern)
{
    g_return_val_if_fail (UFO_IS_PLUGIN_MANAGER (manager), NULL);
    UfoPluginManagerPrivate *priv = UFO_PLUGIN_MANAGER_GET_PRIVATE (manager);
    GList *result = NULL;

    GMatchInfo *match_info = NULL;

    for (GList *path = g_list_first (priv->search_paths); path != NULL; path = g_list_next (path)) {
        glob_t glob_vector;
        gchar *pattern;

        pattern = g_build_filename ((gchar *) path->data, filename_pattern, NULL);
        glob (pattern, GLOB_MARK | GLOB_TILDE, NULL, &glob_vector);
        g_free (pattern);
        gsize i = 0;

        while (i < glob_vector.gl_pathc) {
            g_regex_match (filename_regex, glob_vector.gl_pathv[i], 0, &match_info);

            if (g_match_info_matches (match_info)) {
                gchar *word = g_match_info_fetch (match_info, 1);
                result = g_list_append (result, word);
            }

            i++;
        }
    }

    g_match_info_free (match_info);
    return result;
}

static void
ufo_plugin_manager_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_plugin_manager_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    switch (property_id) {
        case PROP_CONFIG:
            {
                GObject *value_object;

                value_object = g_value_get_object (value);

                if (value_object != NULL) {
                    UfoConfig *config;

                    config = UFO_CONFIG (value_object);
                    copy_config_paths (UFO_PLUGIN_MANAGER_GET_PRIVATE (object), config);
                }
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_plugin_manager_constructed (GObject *object)
{
    UfoPluginManagerPrivate *priv = UFO_PLUGIN_MANAGER_GET_PRIVATE (object);

    if (priv->search_paths == NULL) {
        UfoConfig *config = ufo_config_new ();
        copy_config_paths (priv, config);
        g_object_unref (config);
    }
}

static void
ufo_plugin_manager_dispose (GObject *object)
{
    G_OBJECT_CLASS (ufo_plugin_manager_parent_class)->dispose (object);
    g_debug ("UfoPluginManager: disposed");
}

static void
ufo_plugin_manager_finalize (GObject *gobject)
{
    UfoPluginManager *manager = UFO_PLUGIN_MANAGER (gobject);
    UfoPluginManagerPrivate *priv = UFO_PLUGIN_MANAGER_GET_PRIVATE (manager);

    /* XXX: This is a necessary hack! We return a full reference for
     * ufo_plugin_manager_get_task() so that the Python run-time can cleanup
     * the tasks that are assigned. However, there is no relationship between
     * graphs, tasks and the plugin manager and it might happen, that the plugin
     * manager is destroy before the graph which in turn would unref invalid
     * objects. So, we just don't close the modules and hope for the best.
     */
    /* g_slist_foreach (priv->modules, (GFunc) g_module_close, NULL); */
    /* g_slist_free (priv->modules); */

    g_list_foreach (priv->search_paths, (GFunc) g_free, NULL);
    g_list_free (priv->search_paths);

    g_hash_table_destroy (priv->new_funcs);
    G_OBJECT_CLASS (ufo_plugin_manager_parent_class)->finalize (gobject);
    g_debug ("UfoPluginManager: finalized");
}

static void
ufo_plugin_manager_class_init (UfoPluginManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->get_property = ufo_plugin_manager_get_property;
    gobject_class->set_property = ufo_plugin_manager_set_property;
    gobject_class->constructed  = ufo_plugin_manager_constructed;
    gobject_class->dispose      = ufo_plugin_manager_dispose;
    gobject_class->finalize     = ufo_plugin_manager_finalize;

    g_object_class_override_property (gobject_class, PROP_CONFIG, "config");

    g_type_class_add_private (klass, sizeof (UfoPluginManagerPrivate));
}

static void
ufo_plugin_manager_init (UfoPluginManager *manager)
{
    static const gchar *PATH_VAR = "UFO_PLUGIN_PATH";
    UfoPluginManagerPrivate *priv;

    manager->priv = priv = UFO_PLUGIN_MANAGER_GET_PRIVATE (manager);
    priv->modules = NULL;
    priv->search_paths = NULL;
    priv->new_funcs = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             g_free, g_free);

    if (g_getenv (PATH_VAR)) {
        priv->search_paths = g_list_append (priv->search_paths,
                                            g_strdup (g_getenv (PATH_VAR)));
    }
}



static gchar *
transform_string (const gchar *pattern,
                  const gchar *s,
                  const gchar *separator)
{
    gchar **sv;
    gchar *transformed;
    gchar *result;

    sv = g_strsplit_set (s, "-_ ", -1);
    transformed = g_strjoinv (separator, sv);
    result = g_strdup_printf (pattern, transformed);

    g_strfreev (sv);
    g_free (transformed);
    return result;
}

/**
 * ufo_plugin_manager_get_task:
 * @manager: A #UfoPluginManager
 * @name: Name of the plugin.
 * @error: return location for a GError or %NULL
 *
 * Load a #UfoFilter module and return an instance. The shared object name must
 * be * constructed as "libfilter@name.so".
 *
 * Since: 0.2, the error parameter is available
 *
 * Returns: (transfer full): (allow-none): #UfoFilter or %NULL if module cannot be found
 */
UfoTaskNode *
ufo_plugin_manager_get_task (UfoPluginManager *manager, const gchar *name, GError **error)
{
    g_return_val_if_fail (UFO_IS_PLUGIN_MANAGER (manager) && name != NULL, NULL);
    UfoTaskNode *node;
    if (!g_strcmp0 (name, "[dummy]"))
        return ufo_dummy_task_new ();

    gchar *module_name = transform_string ("libufofilter%s.so", name, NULL);
    gchar *func_name = transform_string ("ufo_%s_task_new", name, "_");
    node = UFO_TASK_NODE (ufo_plugin_manager_get_plugin (manager,
                                          func_name,
                                          module_name,
                                          error));

    ufo_task_node_set_plugin_name (node, name);

    g_free (func_name);
    g_free (module_name);

    g_debug ("UfoPluginManager: Created %s-%p", name, node);
    return node;
}

/**
 * ufo_plugin_manager_get_all_task_names:
 * @manager: A #UfoPluginManager
 *
 * Return a list with potential filter names that match shared objects in all
 * search paths.
 *
 * Return value: (element-type utf8) (transfer full): List of strings with filter names
 */
GList *
ufo_plugin_manager_get_all_task_names (UfoPluginManager *manager)
{
    g_return_val_if_fail (UFO_IS_PLUGIN_MANAGER (manager), NULL);
    GRegex *regex = g_regex_new ("libufofilter([A-Za-z]+).so", 0, 0, NULL);

    GList *result = ufo_plugin_get_all_plugin_names(manager,
                                                    regex,
                                                    "libufofilter*.so");

    g_regex_unref (regex);
    return result;
}
