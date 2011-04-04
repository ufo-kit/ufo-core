#include "ufo-graph.h"
#include "ufo-filter.h"

G_DEFINE_TYPE(UfoGraph, ufo_graph, G_TYPE_OBJECT);

#define UFO_GRAPH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GRAPH, UfoGraphPrivate))

struct _UfoGraphPrivate {

};

static void ufo_graph_class_init(UfoGraphClass *klass)
{
    /* install private data */
    g_type_class_add_private(klass, sizeof(UfoGraphPrivate));
}

static void ufo_graph_init(UfoGraph *self)
{
    /* init public fields */

    /* init private fields */
    UfoGraphPrivate *priv;
    self->priv = priv = UFO_GRAPH_GET_PRIVATE(self);
}


