#include <unistd.h>
#include <dlfcn.h>
#include "ufo-plugin.h"

G_DEFINE_TYPE(UfoPlugin, ufo_plugin, G_TYPE_OBJECT);

#define UFO_PLUGIN_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_PLUGIN, UfoPluginPrivate))

struct _UfoPluginPrivate {
    void *plugin_handle;
    plugin_init init;
    plugin_destroy destroy;
    plugin_get_filter_names get_filter_names;
    plugin_get_filter_description get_filter_description;
};

/*
 * private functions
 */
static gboolean ufo_plugin_create(UfoPlugin *plugin, const gchar *file_name)
{
    UfoPluginPrivate *priv = UFO_PLUGIN_GET_PRIVATE(plugin);

    if (access(file_name, F_OK) == 0) {
        priv->plugin_handle = dlopen(file_name, RTLD_NOW);
        if (priv->plugin_handle != NULL) {
            priv->init = dlsym(priv->plugin_handle, "plugin_init");
            if (dlerror())
                return FALSE;

            priv->destroy = dlsym(priv->plugin_handle, "plugin_destroy");
            if (dlerror())
                return FALSE;

            priv->get_filter_names = dlsym(priv->plugin_handle, "plugin_get_filter_names");
            if (dlerror())
                return FALSE;

            priv->get_filter_description = dlsym(priv->plugin_handle, "plugin_get_filter_description");
            if (dlerror())
                return FALSE;
        }
        else
            return FALSE;
    }
    return TRUE;
}

/* 
 * non-virtual public methods 
 */

/*
 * \brief Create a new buffer with given dimensions
 *
 * \param[in] file_name File name of shared object
 *
 * \return Plugin object or NULL if shared object couldn't be loaded
 */
UfoPlugin *ufo_plugin_new(const gchar *file_name)
{   
    UfoPlugin *plugin = g_object_new(UFO_TYPE_PLUGIN, NULL);
    if (!ufo_plugin_create(plugin, file_name)) {
        g_object_unref(plugin);
        plugin = NULL;
    }
    return plugin;
}

/*
 * \brief Retrieve a brief description of a filter
 * \parameter[in] filter_name Name of the filter
 * \return Description string
 */
const gchar *ufo_get_filter_description(UfoPlugin *plugin, const gchar *filter_name)
{
    UfoPluginPrivate *priv = UFO_PLUGIN_GET_PRIVATE(plugin);
    return priv->get_filter_description(filter_name);
}

/*
 * \brief Create a UfoFilter based on a plugin filter name
 * \parameter[in] name Name of the filter
 * \return UfoFilter object calling the plugin's filter
 */
UfoFilter *ufo_get_filter(UfoPlugin *plugin, const gchar *name)
{
    UfoPluginPrivate *priv = UFO_PLUGIN_GET_PRIVATE(plugin);
    plugin_filter_call call = dlsym(priv->plugin_handle, name);
    if (dlerror())
        return NULL;

    /* TODO: create a UfoFilterPlugin object and set the function pointer */
    call = NULL;
    return NULL;
}


/* 
 * virtual methods 
 */

static void ufo_plugin_dispose(GObject *gobject)
{
    /*UfoPlugin *self = UFO_PLUGIN(gobject);*/
    
    G_OBJECT_CLASS(ufo_plugin_parent_class)->dispose(gobject);
}

static void ufo_plugin_finalize(GObject *gobject)
{
    UfoPlugin *self = UFO_PLUGIN(gobject);
    UfoPluginPrivate *priv = UFO_PLUGIN_GET_PRIVATE(self);

    dlclose(priv->plugin_handle);

    G_OBJECT_CLASS(ufo_plugin_parent_class)->finalize(gobject);
}

static void ufo_plugin_class_init(UfoPluginClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose = ufo_plugin_dispose;
    gobject_class->finalize = ufo_plugin_finalize;

    g_type_class_add_private(klass, sizeof(UfoPluginPrivate));
}

static void ufo_plugin_init(UfoPlugin *self)
{
    /* init public fields */

    /* init private fields */
    UfoPluginPrivate *priv;
    self->priv = priv = UFO_PLUGIN_GET_PRIVATE(self);
}
