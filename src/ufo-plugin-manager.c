
#include <gmodule.h>
#include "ufo-plugin-manager.h"


G_DEFINE_TYPE(UfoPluginManager, ufo_plugin_manager, G_TYPE_OBJECT);

#define UFO_PLUGIN_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_PLUGIN_MANAGER, UfoPluginManagerPrivate))

typedef UfoFilter* (* GetFilterFunc) (void);

struct _UfoPluginManagerPrivate {
    GSList *search_paths;
    GSList *modules;
    GHashTable *filter_funcs;  /**< maps from gchar* to GetFilterFunc* */
};


static gchar *plugin_manager_get_path(UfoPluginManagerPrivate *priv, const gchar *name)
{
    /* Check first if filename is already a path */
    if (g_path_is_absolute(name)) {
        if (g_file_test(name, G_FILE_TEST_EXISTS))
            return g_strdup(name);
        else
            return NULL;
    }

    /* If it is not a path, search in all known paths */
    GSList *p = g_slist_nth(priv->search_paths, 0);

    while (p != NULL) {
        gchar *path = g_strdup_printf("%s%c%s", (gchar *) p->data, G_DIR_SEPARATOR, name);
        if (g_file_test(path, G_FILE_TEST_EXISTS))
            return path;
        g_free(path);
        p = g_slist_next(p);
    }

    return NULL;
}

/**
 * ufo_filter_plugin_new: Create a new plugin manager object
 */
UfoPluginManager *ufo_plugin_manager_new(void)
{
    UfoPluginManager *manager = UFO_PLUGIN_MANAGER(g_object_new(UFO_TYPE_PLUGIN_MANAGER, NULL));
    return manager;
}

/**
 * ufo_plugin_manager_add_paths: Add paths from which to search for modules
 * @paths: (in): string with colon-separated list of absolute paths
 */
void ufo_plugin_manager_add_paths(UfoPluginManager *manager, const gchar *paths)
{
    UfoPluginManagerPrivate *priv = UFO_PLUGIN_MANAGER_GET_PRIVATE(manager);
    gchar **path_list = g_strsplit(paths, ":", 0);
    gchar **p = path_list;
    while (*p != NULL)
        priv->search_paths = g_slist_append(priv->search_paths, g_strdup(*(p++)));
    g_strfreev(path_list);
}

/**
 * ufo_plugin_manager_get_filter: Load a UfoFilter module and return an instance
 * @returns: UfoFilter or NULL if module cannot be found
 */
UfoFilter *ufo_plugin_manager_get_filter(UfoPluginManager *manager, const gchar *name)
{
    UfoPluginManagerPrivate *priv = UFO_PLUGIN_MANAGER_GET_PRIVATE(manager);
    GetFilterFunc *func = NULL;
    GModule *module = NULL;

    if (name == NULL)
        return NULL;

    func = g_hash_table_lookup(priv->filter_funcs, name); 
    if (func != NULL) 
        return (*func)(); 

    /* No suitable function found, let's find the module */
    gchar *module_name = g_strdup_printf("%s.so", name);
    gchar *path = plugin_manager_get_path(priv, module_name);
    g_free(module_name);
    if (path == NULL)
        return NULL;

    module = g_module_open(path, G_MODULE_BIND_LAZY);
    g_free(path);
    if (!module) {
        g_error("%s", g_module_error());
        return NULL;
    }

    func = g_malloc0(sizeof(GetFilterFunc));
    if (!g_module_symbol(module, "ufo_filter_plugin_new", (gpointer *) func)) {
        g_error("%s", g_module_error()); 
        g_free(func);
        if (!g_module_close(module))
            g_warning("%s", g_module_error());
        return NULL;
    }

    priv->modules = g_slist_append(priv->modules, module);
    g_hash_table_insert(priv->filter_funcs, g_strdup(name), func);
    return (*func)();
}

static void ufo_plugin_manager_finalize(GObject *gobject)
{
    UfoPluginManager *manager = UFO_PLUGIN_MANAGER(gobject);
    UfoPluginManagerPrivate *priv = UFO_PLUGIN_MANAGER_GET_PRIVATE(manager);

    GSList *module = g_slist_nth(priv->modules, 0);
    while (module != NULL) {
        g_module_close(module->data);
        module = g_slist_next(module);
    }

    g_slist_free(priv->modules);

    g_slist_foreach(priv->search_paths, (GFunc) g_free, NULL);
    g_slist_free(priv->search_paths);
    g_hash_table_destroy(priv->filter_funcs);

    G_OBJECT_CLASS(ufo_plugin_manager_parent_class)->finalize(gobject);
}

static void ufo_plugin_manager_class_init(UfoPluginManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = ufo_plugin_manager_finalize;

    g_type_class_add_private(klass, sizeof(UfoPluginManagerPrivate));
}

static void ufo_plugin_manager_init(UfoPluginManager *manager)
{
    UfoPluginManagerPrivate *priv;
    manager->priv = priv = UFO_PLUGIN_MANAGER_GET_PRIVATE(manager);
    priv->search_paths = NULL;
    priv->modules = NULL;
    priv->filter_funcs = g_hash_table_new_full(g_str_hash, g_str_equal,
            g_free, g_free);
}

