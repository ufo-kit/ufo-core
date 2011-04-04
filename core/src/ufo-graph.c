#include <glib.h>
#include "ufo-graph.h"
#include "ufo-filter.h"

G_DEFINE_TYPE(UfoGraph, ufo_graph, G_TYPE_OBJECT);

#define UFO_GRAPH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GRAPH, UfoGraphPrivate))

struct _UfoGraphPrivate {
    UfoFilter  *root;
    GHashTable *graph;
};

void ufo_graph_set_root(UfoGraph *self, UfoFilter *filter)
{
    self->priv->root = filter;
    g_hash_table_insert(self->priv->graph, filter, NULL);
}

void ufo_graph_connect(UfoGraph *self, UfoFilter *src, UfoFilter *dst)
{
    g_hash_table_replace(self->priv->graph, src, dst);
}

static void ufo_graph_dispose(GObject *gobject)
{
    UfoGraph *self = UFO_GRAPH(gobject);
    
    if (self->priv->graph) {
        g_object_unref(self->priv->graph);
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

static void ufo_graph_init(UfoGraph *self)
{
    /* init public fields */

    /* init private fields */
    UfoGraphPrivate *priv;
    self->priv = priv = UFO_GRAPH_GET_PRIVATE(self);
    priv->graph = g_hash_table_new(NULL, NULL);
}


