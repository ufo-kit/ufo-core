#include <glib.h>
#include <ethos/ethos.h>
#include "ufo-graph.h"
#include "ufo-filter.h"

G_DEFINE_TYPE(UfoGraph, ufo_graph, G_TYPE_OBJECT);

#define UFO_GRAPH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GRAPH, UfoGraphPrivate))

struct _UfoGraphPrivate {
    EthosManager    *ethos;
    UfoFilter       *root;
    GHashTable      *graph;
};

void ufo_graph_set_root(UfoGraph *self, UfoFilter *filter)
{
    self->priv->root = filter;
    g_hash_table_insert(self->priv->graph, filter, NULL);
}

void ufo_graph_connect(UfoGraph *self, UfoFilter *src, UfoFilter *dst)
{
    g_hash_table_replace(self->priv->graph, src, dst);
    g_hash_table_replace(self->priv->graph, dst, NULL);
}

void ufo_graph_run(UfoGraph *self)
{
    UfoFilter *node = self->priv->root;
    while (node != NULL) {
        node = g_hash_table_lookup(self->priv->graph, node);
    }
}

UfoGraph *ufo_graph_new()
{
    return g_object_new(UFO_TYPE_GRAPH, NULL);
}

static void ufo_graph_dispose(GObject *gobject)
{
    UfoGraph *self = UFO_GRAPH(gobject);
    
    if (self->priv->graph) {
        g_hash_table_destroy(self->priv->graph);
        self->priv->graph = NULL;
    }

    G_OBJECT_CLASS(ufo_graph_parent_class)->dispose(gobject);
}

static void ufo_graph_class_init(UfoGraphClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose = ufo_graph_dispose;

    /* install private data */
    g_type_class_add_private(klass, sizeof(UfoGraphPrivate));
}

static void ufo_graph_show_plugin(gpointer data, gpointer user_data)
{
    EthosPluginInfo *info = (EthosPluginInfo *) data;
    g_message("Plugin Name: %s\n", ethos_plugin_info_get_name(info));
    g_message("Description: %s\n", ethos_plugin_info_get_description(info));
}

static void ufo_graph_init(UfoGraph *self)
{
    /* init public fields */

    /* init private fields */
    UfoGraphPrivate *priv;
    self->priv = priv = UFO_GRAPH_GET_PRIVATE(self);

    /* TODO: determine directories in a better way */
    gchar *plugin_dirs[] = { "/usr/local/lib", "../filters", NULL };

    priv->ethos = ethos_manager_new_full("UFO", plugin_dirs);
    ethos_manager_initialize(priv->ethos);

    GList *plugin_info = ethos_manager_get_plugin_info(priv->ethos);
    g_list_foreach(plugin_info, &ufo_graph_show_plugin, NULL);
    g_list_free(plugin_info);

    priv->graph = g_hash_table_new(NULL, NULL);
}


