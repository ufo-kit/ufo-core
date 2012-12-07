/**
 * SECTION:ufo-plugin-manager
 * @Short_description: Load an #UfoFilter from a shared object
 * @Title: UfoPluginManager
 *
 * The plugin manager opens shared object modules searched for in locations
 * specified with ufo_plugin_manager_add_paths(). An #UfoFilter can be
 * instantiated with ufo_plugin_manager_get_filter() with a one-to-one mapping
 * between filter name xyz and module name libfilterxyz.so. Any errors are
 * reported as one of #UfoPluginManagerError codes.
 */
#include <gmodule.h>
#include <glob.h>
#include <ufo-plugin-manager.h>
#include <ufo-configurable.h>
#include "config.h"

G_DEFINE_TYPE_WITH_CODE (UfoPluginManager, ufo_plugin_manager, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_CONFIGURABLE, NULL))

#define UFO_PLUGIN_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_PLUGIN_MANAGER, UfoPluginManagerPrivate))

typedef UfoNode* (* NewFunc) (void);

struct _UfoPluginManagerPrivate {
    GSList      *search_paths;
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
    GSList *p = g_slist_nth (priv->search_paths, 0);

    while (p != NULL) {
        gchar *path = g_build_filename ((gchar *) p->data, name, NULL);

        if (g_file_test (path, G_FILE_TEST_EXISTS))
            return path;

        g_free (path);
        p = g_slist_next (p);
    }

    return NULL;
}

static void
copy_config_paths (UfoPluginManagerPrivate *priv, UfoConfig *config)
{
    gchar **paths;

    paths = ufo_config_get_paths (config);

    for (guint i = 0; paths[i] != NULL; i++)
        priv->search_paths = g_slist_prepend (priv->search_paths, g_strdup (paths[i]));

    g_strfreev (paths);
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
                                                "configuration", config,
                                                NULL));

    return manager;
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
 * ufo_plugin_manager_get_filter:
 * @manager: A #UfoPluginManager
 * @name: Name of the plugin.
 * @error: return location for a GError or %NULL
 *
 * Load a #UfoFilter module and return an instance. The shared object name must
 * be * constructed as "libfilter@name.so".
 *
 * Returns: (transfer none) (allow-none): #UfoFilter or %NULL if module cannot be found
 *
 * Since: 0.2, the error parameter is available
 */
UfoNode *
ufo_plugin_manager_get_task (UfoPluginManager *manager, const gchar *name, GError **error)
{
    g_return_val_if_fail (UFO_IS_PLUGIN_MANAGER (manager) && (name != NULL), NULL);
    UfoPluginManagerPrivate *priv = UFO_PLUGIN_MANAGER_GET_PRIVATE (manager);
    UfoNode *node;
    NewFunc *func;
    GModule *module;
    gchar *func_name = NULL;
    gchar *module_name = NULL;

    func = g_hash_table_lookup (priv->new_funcs, name);

    if (func == NULL) {
        module_name = transform_string ("libufofilter%s.so", name, NULL);
        func_name = transform_string ("ufo_%s_task_new", name, "_");

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
        g_hash_table_insert (priv->new_funcs, g_strdup (name), func);

        g_free (func_name);
        g_free (module_name);
    }

    node = (*func) ();
    /* ufo_filter_set_plugin_name (filter, name); */
    g_message ("UfoPluginManager: Created %s-%p", name, (gpointer) node);

    return node;

handle_error:
    g_free (module_name);
    g_free (func_name);
    return NULL;
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
    UfoPluginManagerPrivate *priv = UFO_PLUGIN_MANAGER_GET_PRIVATE (manager);
    GList *result = NULL;
    GSList *path;

    GRegex *regex = g_regex_new ("libufofilter([A-Za-z]+).so", 0, 0, NULL);
    GMatchInfo *match_info = NULL;

    path = g_slist_nth (priv->search_paths, 0);

    while (path != NULL) {
        glob_t glob_vector;
        gchar *pattern;

        pattern = g_build_filename ((gchar *) path->data, "libufofilter*.so", NULL);
        glob (pattern, GLOB_MARK | GLOB_TILDE, NULL, &glob_vector);
        g_free (pattern);
        gsize i = 0;

        while (i < glob_vector.gl_pathc) {
            g_regex_match (regex, glob_vector.gl_pathv[i], 0, &match_info);

            if (g_match_info_matches (match_info)) {
                gchar *word = g_match_info_fetch (match_info, 1);
                result = g_list_append (result, word);
            }

            i++;
        }

        path = g_slist_next (path);
    }

    g_match_info_free (match_info);
    g_regex_unref (regex);
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
    g_message ("UfoPluginManager: disposed");
}

static void
ufo_plugin_manager_finalize (GObject *gobject)
{
    UfoPluginManager *manager = UFO_PLUGIN_MANAGER (gobject);
    UfoPluginManagerPrivate *priv = UFO_PLUGIN_MANAGER_GET_PRIVATE (manager);
    GSList *module = g_slist_nth (priv->modules, 0);

    while (module != NULL) {
        g_module_close (module->data);
        module = g_slist_next (module);
    }

    g_slist_free (priv->modules);

    g_slist_foreach (priv->search_paths, (GFunc) g_free, NULL);
    g_slist_free (priv->search_paths);

    g_hash_table_destroy (priv->new_funcs);
    G_OBJECT_CLASS (ufo_plugin_manager_parent_class)->finalize (gobject);
    g_message ("UfoPluginManager: finalized");
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
    UfoPluginManagerPrivate *priv;

    manager->priv = priv = UFO_PLUGIN_MANAGER_GET_PRIVATE (manager);
    priv->modules = NULL;
    priv->search_paths = NULL;
    priv->new_funcs = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             g_free, g_free);
}
