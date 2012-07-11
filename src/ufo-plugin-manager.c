
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
#include "ufo-plugin-manager.h"


G_DEFINE_TYPE(UfoPluginManager, ufo_plugin_manager, G_TYPE_OBJECT)

#define UFO_PLUGIN_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_PLUGIN_MANAGER, UfoPluginManagerPrivate))

typedef UfoFilter* (* GetFilterFunc) (void);

struct _UfoPluginManagerPrivate {
    GSList *search_paths;
    GSList *modules;
    GHashTable *filter_funcs;  /**< maps from gchar* to GetFilterFunc* */
};

enum {
    PROP_0,
    PROP_PATHS,
    N_PROPERTIES
};

static GParamSpec *plugin_manager_properties[N_PROPERTIES] = { NULL, };

/**
 * UfoPluginManagerError:
 * @UFO_PLUGIN_MANAGER_ERROR_MODULE_NOT_FOUND: The module could not be found
 * @UFO_PLUGIN_MANAGER_ERROR_MODULE_OPEN: Module could not be opened
 * @UFO_PLUGIN_MANAGER_ERROR_SYMBOL_NOT_FOUND: Necessary entry symbol was not
 *      found
 *
 * Possible errors that ufo_plugin_manager_get_filter() can return.
 */
GQuark ufo_plugin_manager_error_quark (void)
{
    return g_quark_from_static_string ("ufo-plugin-manager-error-quark");
}

static gchar *plugin_manager_get_path (UfoPluginManagerPrivate *priv, const gchar *name)
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
        gchar *path = g_strdup_printf ("%s%c%s", (gchar *) p->data, G_DIR_SEPARATOR, name);

        if (g_file_test (path, G_FILE_TEST_EXISTS))
            return path;

        g_free (path);
        p = g_slist_next (p);
    }

    return NULL;
}

static void add_paths (UfoPluginManager *manager, const gchar *paths)
{
    if (paths != NULL) {
        gchar **path_list = g_strsplit (paths, ":", 0);
        gchar **p = path_list;

        while (*p != NULL)
            manager->priv->search_paths = g_slist_prepend (manager->priv->search_paths, g_strdup (*(p++)));

        g_strfreev (path_list);
    }
}

/**
 * ufo_plugin_manager_new:
 * @paths: (allow-none): String with a colon-separated list of additional
 *      paths to search for plugins or %NULL.
 *
 * Create a plugin manager object to instantiate filter objects.
 *
 * Return value: A new plugin manager object.
 */
UfoPluginManager *ufo_plugin_manager_new (const gchar *paths)
{
    UfoPluginManager *manager = UFO_PLUGIN_MANAGER (g_object_new (UFO_TYPE_PLUGIN_MANAGER, 
            "paths", paths, 
            NULL));

    return manager;
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
 * Return value (transfer full) (allow-none): #UfoFilter or %NULL if module cannot be found
 *
 * Since: 0.2, the error parameter is available
 */
UfoFilter *ufo_plugin_manager_get_filter (UfoPluginManager *manager, const gchar *name, GError **error)
{
    g_return_val_if_fail (UFO_IS_PLUGIN_MANAGER (manager) || (name != NULL), NULL);
    UfoPluginManagerPrivate *priv = UFO_PLUGIN_MANAGER_GET_PRIVATE (manager);
    UfoFilter *filter;
    GetFilterFunc *func = NULL;
    GModule *module = NULL;
    gchar *module_name = NULL;
    const gchar *entry_symbol_name = "ufo_filter_plugin_new"; 

    func = g_hash_table_lookup (priv->filter_funcs, name);

    if (func == NULL) {
        /* No suitable function found, let's find the module */
        module_name = g_strdup_printf ("libufofilter%s.so", name);
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

        func = g_malloc0 (sizeof (GetFilterFunc));

        if (!g_module_symbol (module, entry_symbol_name, (gpointer *) func)) {
            g_set_error (error, UFO_PLUGIN_MANAGER_ERROR, UFO_PLUGIN_MANAGER_ERROR_SYMBOL_NOT_FOUND,
                    "%s is not exported by module %s: %s", entry_symbol_name, module_name, g_module_error ());
            g_free (func);

            if (!g_module_close (module))
                g_warning ("%s", g_module_error ());

            goto handle_error;
        }

        priv->modules = g_slist_append (priv->modules, module);
        g_hash_table_insert (priv->filter_funcs, g_strdup (name), func);
        g_free (module_name);
    }

    filter = (*func) ();
    ufo_filter_set_plugin_name (filter, name);

    return filter;

handle_error:
    g_free (module_name);
    return NULL;
}

/**
 * ufo_plugin_manager_available_filters:
 * @manager: A #UfoPluginManager
 *
 * Return a list with potential filter names that match shared objects in all
 * search paths.
 *
 * Return value: (element-type utf8) (transfer full): List of strings with filter names
 */
GList *ufo_plugin_manager_available_filters (UfoPluginManager *manager)
{
    g_return_val_if_fail (UFO_IS_PLUGIN_MANAGER (manager), NULL);
    UfoPluginManagerPrivate *priv = UFO_PLUGIN_MANAGER_GET_PRIVATE (manager);
    GList *result = NULL;
    GSList *path = g_slist_nth (priv->search_paths, 0);

    GRegex *regex = g_regex_new ("libufofilter ([A-Za-z]+).so", 0, 0, NULL);
    GMatchInfo *match_info = NULL;

    while (path != NULL) {
        glob_t glob_vector;
        gchar *pattern = g_strdup_printf ("%s%clibufofilter*.so", (gchar *) path->data, G_DIR_SEPARATOR);
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

static void ufo_plugin_manager_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void ufo_plugin_manager_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    switch (property_id) {
        case PROP_PATHS:
            add_paths (UFO_PLUGIN_MANAGER (object), g_value_get_string (value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void ufo_plugin_manager_finalize (GObject *gobject)
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
    g_hash_table_destroy (priv->filter_funcs);
    G_OBJECT_CLASS (ufo_plugin_manager_parent_class)->finalize (gobject);
}

static void ufo_plugin_manager_class_init (UfoPluginManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->get_property = ufo_plugin_manager_get_property;
    gobject_class->set_property = ufo_plugin_manager_set_property;
    gobject_class->finalize = ufo_plugin_manager_finalize;

    plugin_manager_properties[PROP_PATHS] =
        g_param_spec_string ("paths",
                "List of colong-separated paths",
                "List of colong-separated paths pointing to filter locations",
                ".",
                G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);

    g_object_class_install_property (gobject_class, PROP_PATHS, plugin_manager_properties[PROP_PATHS]);
    g_type_class_add_private (klass, sizeof (UfoPluginManagerPrivate));
}

static void ufo_plugin_manager_init (UfoPluginManager *manager)
{
    UfoPluginManagerPrivate *priv;
    manager->priv = priv = UFO_PLUGIN_MANAGER_GET_PRIVATE (manager);
    priv->search_paths = NULL;
    priv->modules = NULL;
    priv->filter_funcs = g_hash_table_new_full (g_str_hash, g_str_equal,
            g_free, g_free);
}

