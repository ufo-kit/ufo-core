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
#include <ufo/ufo-task-node.h>
#include <ufo/ufo-dummy-task.h>
#include "compat.h"

/**
 * SECTION:ufo-plugin-manager
 * @Short_description: Load a task from a shared object
 * @Title: UfoPluginManager
 *
 * The plugin manager opens and loads #UfoTaskNode objects using
 * ufo_plugin_manager_get_task() from shared objects.  The libraries are
 * searched for in the path configured at build time and in paths provided by
 * the UFO_PLUGIN_PATH environment variable. The name of the plugin xyz maps to
 * the library name libufofilterxyz.so.
 */

gchar *ufo_transform_string (const gchar *pattern, const gchar *s, const gchar *separator);
gchar *ufo_transform_string2 (const gchar *pattern, const gchar *s1,const gchar *s2, const gchar *separator);

G_DEFINE_TYPE (UfoPluginManager, ufo_plugin_manager, G_TYPE_OBJECT)

#define UFO_PLUGIN_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_PLUGIN_MANAGER, UfoPluginManagerPrivate))

typedef UfoNode* (* NewFunc) (void);

struct _UfoPluginManagerPrivate {
    GList       *paths;
    GSList      *modules;
    GHashTable  *new_funcs;     /* maps from gchar* to NewFunc* */
};

enum {
    PROP_0,
    N_PROPERTIES
};

/**
 * UfoPluginManagerError:
 * @UFO_PLUGIN_MANAGER_ERROR_MODULE_NOT_FOUND: The module could not be found
 * @UFO_PLUGIN_MANAGER_ERROR_MODULE_OPEN: Module could not be opened
 * @UFO_PLUGIN_MANAGER_ERROR_SYMBOL_NOT_FOUND: Necessary entry symbol was not
 *      found
 *
 * Possible errors that ufo_plugin_manager_get_task() can return.
 */
GQuark
ufo_plugin_manager_error_quark (void)
{
    return g_quark_from_static_string ("ufo-plugin-manager-error-quark");
}

static gchar *
plugin_manager_get_path (UfoPluginManagerPrivate *priv, const gchar *name)
{
    GList *it;

    /* Check first if filename is already a path */
    if (g_path_is_absolute (name)) {
        if (g_file_test (name, G_FILE_TEST_EXISTS))
            return g_strdup (name);
        else
            return NULL;
    }

    /* If it is not a path, search in all known paths */
    g_list_for (priv->paths, it) {
        gchar *path = g_build_filename ((gchar *) it->data, name, NULL);

        if (g_file_test (path, G_FILE_TEST_EXISTS))
            return path;

        g_free (path);
    }

    return NULL;
}

/**
 * ufo_plugin_manager_new:
 *
 * Create a plugin manager object to instantiate filter objects.
 *
 * Return value: A new #UfoPluginManager object.
 */
UfoPluginManager *
ufo_plugin_manager_new (void)
{
    return UFO_PLUGIN_MANAGER (g_object_new (UFO_TYPE_PLUGIN_MANAGER, NULL));
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
 * Returns: (transfer full): (allow-none): A loaded plugin or %NULL if module
 * cannot be found.
 */
GObject *
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
    UfoPluginManagerPrivate *priv;
    GList *it;
    GList *result = NULL;
    GMatchInfo *match_info = NULL;

    g_return_val_if_fail (UFO_IS_PLUGIN_MANAGER (manager), NULL);
    priv = manager->priv;

    g_list_for (priv->paths, it) {
        glob_t glob_vector;
        gchar *pattern;

        pattern = g_build_filename ((gchar *) it->data, filename_pattern, NULL);
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

    g_list_free_full (priv->paths, g_free);

    g_hash_table_destroy (priv->new_funcs);
    G_OBJECT_CLASS (ufo_plugin_manager_parent_class)->finalize (gobject);
}

static void
ufo_plugin_manager_class_init (UfoPluginManagerClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);
    oclass->finalize = ufo_plugin_manager_finalize;

    g_type_class_add_private (klass, sizeof (UfoPluginManagerPrivate));
}

static void
add_environment_paths (UfoPluginManagerPrivate *priv, const gchar *env)
{
    gchar **paths;

    if (env == NULL)
        return;

    paths = g_strsplit(env, ":", -1);

    for (unsigned idx = 0; paths[idx]; ++idx) {
        /* Ignore empty paths */
        if (*paths[idx])
            priv->paths = g_list_append (priv->paths, paths[idx]);
    }

    g_free(paths);
}

static void
ufo_plugin_manager_init (UfoPluginManager *manager)
{
    static const gchar *PATH_VAR = "UFO_PLUGIN_PATH";
    UfoPluginManagerPrivate *priv;

    manager->priv = priv = UFO_PLUGIN_MANAGER_GET_PRIVATE (manager);
    priv->modules = NULL;
    priv->new_funcs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    add_environment_paths (priv, g_getenv (PATH_VAR));
    priv->paths = g_list_append (priv->paths, g_strdup (UFO_PLUGIN_DIR));
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
        return UFO_TASK_NODE (ufo_dummy_task_new ());

    gchar *module_name = ufo_transform_string ("libufofilter%s.so", name, NULL);
    gchar *func_name = ufo_transform_string ("ufo_%s_task_new", name, "_");
    node = UFO_TASK_NODE (ufo_plugin_manager_get_plugin (manager,
                                          func_name,
                                          module_name,
                                          error));

    ufo_task_node_set_plugin_name (node, name);

    g_free (func_name);
    g_free (module_name);

    g_debug ("UfoPluginManager: Created %s-%p", name, (gpointer) node);
    return node;
}

/**
 * ufo_plugin_manager_get_task_from_package:
 * @manager: A #UfoPluginManager
 * @package_name: Name of library package
 * @name: Name of the plugin.
 * @error: return location for a GError or %NULL
 *
 * Load a #UfoTaskNode module and return an instance. The shared object name must
 * be in @package_name subfolder and constructed as "lib@name.so".
 *
 * Since: 0.2, the error parameter is available
 *
 * Returns: (transfer full): (allow-none): #UfoTaskNode or %NULL if module cannot be found
 */
UfoTaskNode*
ufo_plugin_manager_get_task_from_package(UfoPluginManager   *manager,
                                         const gchar        *package_name,
                                         const gchar        *name,
                                         GError            **error)
{
    g_return_val_if_fail (UFO_IS_PLUGIN_MANAGER (manager) && name != NULL, NULL);

    gchar *so_name = ufo_transform_string2("%s/libufo%s.so", package_name, name, NULL);
    gchar *func_name = ufo_transform_string2("ufo_%s_%s_task_new", package_name, name, "_");

    UfoTaskNode *node = UFO_TASK_NODE (ufo_plugin_manager_get_plugin (manager, func_name, so_name, error));

    ufo_task_node_set_plugin_name (node, name);

    g_free (func_name);
    g_free (so_name);

    g_debug ("UfoPluginManager: Created %s-%p", name, (gpointer) node);
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

/**
 * ufo_transform_string:
 * @pattern: A pattern to place the result string in it.
 * @s: A string, which should be placed in the @pattern.
 * @separator: A string containing separator symbols in the @s that should be removed
 *
 * Returns: A string there in @pattern was placed @s.
 */
gchar *
ufo_transform_string (const gchar *pattern,
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
 * ufo_transform_string2:
 * @pattern: A pattern to place the result string in it.
 * @s: A string, which should be placed in the @pattern.
 * @separator: A string containing separator symbols in the @s that should be removed
 *
 * Returns: A string there in @pattern was placed @s.
 */
gchar *
ufo_transform_string2 (const gchar *pattern,
                       const gchar *s1,
                       const gchar *s2,
                       const gchar *separator)
{
    gchar **sv;
    gchar *transformed1;
    gchar *transformed2;
    gchar *result;
    sv = g_strsplit_set (s1, "-_ ", -1);
    transformed1 = g_strjoinv (separator, sv);
    g_strfreev (sv);
    sv = g_strsplit_set (s2, "-_ ", -1);
    transformed2 = g_strjoinv (separator, sv);
    g_strfreev (sv);
    result = g_strdup_printf (pattern, transformed1, transformed2);
    g_free (transformed1);
    g_free (transformed2);
    return result;
}
