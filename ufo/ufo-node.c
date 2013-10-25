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

#include <ufo/ufo-node.h>
#include <ufo/ufo-graph.h>

/**
 * SECTION:ufo-node
 * @Short_description: Generic node that can be connected in a #UfoGraph
 * @Title: UfoNode
 */

G_DEFINE_TYPE (UfoNode, ufo_node, G_TYPE_OBJECT)

#define UFO_NODE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_NODE, UfoNodePrivate))

struct _UfoNodePrivate {
    UfoNode  *copied_from;
    UfoGraph *graph;
    guint     index;
    guint     total;
    gpointer  label;
};

enum {
    PROP_0,
    PROP_GRAPH,
    N_PROPERTIES
};
static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_node_new (gpointer label)
{
    UfoNode *node;

    node = UFO_NODE (g_object_new (UFO_TYPE_NODE, NULL));
    node->priv->label = label;
    return node;
}

/**
 * ufo_node_get_label:
 * @node: A #UfoNode
 *
 * Get arbitrary label data of @node.
 *
 * Returns: (transfer none): The label of @node.
 */
gpointer
ufo_node_get_label (UfoNode *node)
{
    g_return_val_if_fail (UFO_IS_NODE (node), NULL);
    return node->priv->label;
}

static void
copy_properties (GObject *dst,
                 GObject *src)
{
    GParamSpec **props;
    guint n_props;

    props = g_object_class_list_properties (G_OBJECT_GET_CLASS (src), &n_props);

    for (guint i = 0; i < n_props; i++) {
        GValue value = {0};

        g_value_init (&value, props[i]->value_type);
        g_object_get_property (G_OBJECT (src), props[i]->name, &value);
        g_object_set_property (G_OBJECT (dst), props[i]->name, &value);
    }

    g_free (props);
}

static UfoNode *
ufo_node_copy_real (UfoNode *node,
                    GError **error)
{
    GObject *copy;
    copy = g_object_new (G_OBJECT_TYPE (node), NULL);
    copy_properties (copy, G_OBJECT (node));
    return UFO_NODE (copy);
}

static gboolean
ufo_node_equal_real (UfoNode *n1,
                     UfoNode *n2)
{
    g_return_val_if_fail (UFO_IS_NODE (n1) && UFO_IS_NODE (n2), FALSE);

    /* FIXME: When done we should just check if the types match */
    return n1 == n2 ||
           n1->priv->copied_from == n2 ||
           n2->priv->copied_from == n1;
}

static void
update_total (UfoNode *node,
              guint total)
{
    node->priv = UFO_NODE_GET_PRIVATE (node);
    node->priv->total = total;

    if (node->priv->copied_from != NULL)
        update_total (node->priv->copied_from, total);
}

/**
 * ufo_node_copy:
 * @node: A #UfoNode
 * @error: Location for an error
 *
 * Get a copy of @node. How "deep" the copy is, depends on the inherited
 * implementation of @node. The copy receives an new index and the total amount
 * of nodes is increased by one.
 *
 * Returns: (transfer full): Copy of @node.
 */
UfoNode *
ufo_node_copy (UfoNode *node,
               GError **error)
{
    UfoNode *offspring;

    offspring = UFO_NODE_GET_CLASS (node)->copy (node, error);
    offspring->priv->label = node->priv->label;
    offspring->priv->copied_from = node;
    offspring->priv->graph = node->priv->graph;
    offspring->priv->index = node->priv->total;
    offspring->priv->total = offspring->priv->index + 1;

    update_total (node, offspring->priv->total);

    return offspring;
}

/**
 * ufo_node_get_index:
 * @node: A #UfoNode
 *
 * Get the index of this node. When a graph is expanded, nodes are copied. The
 * original node has index 1, all successive copies receive a monotonous
 * increasing index. The total amount of copied nodes can be queried with
 * ufo_node_get_total().
 *
 * Returns: The index of @node.
 */
guint
ufo_node_get_index (UfoNode *node)
{
    g_return_val_if_fail (UFO_IS_NODE (node), 0);
    return node->priv->index;
}

/**
 * ufo_node_get_total:
 * @node: A #UfoNode
 *
 * Get the total amount of copied nodes.
 *
 * Returns: The number of copied nodes.
 */
guint
ufo_node_get_total (UfoNode *node)
{
    g_return_val_if_fail (UFO_IS_NODE (node), 0);
    return node->priv->total;
}

gboolean
ufo_node_equal (UfoNode *n1,
                UfoNode *n2)
{
    return UFO_NODE_GET_CLASS (n1)->equal (n1, n2);
}

static void
ufo_node_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    UfoNodePrivate *priv = UFO_NODE_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_GRAPH:
            {
                UfoGraph *g = UFO_GRAPH (g_value_get_object (value));
                priv->graph = g;
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_node_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    UfoNodePrivate *priv = UFO_NODE_GET_PRIVATE (object);
    switch (property_id) {
        case PROP_GRAPH:
            g_value_set_object (value, priv->graph);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_node_dispose (GObject *object)
{
}

static void
ufo_node_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_node_parent_class)->finalize (object);
}

static void
ufo_node_class_init (UfoNodeClass *klass)
{
    klass->copy = ufo_node_copy_real;
    klass->equal = ufo_node_equal_real;

    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_node_set_property;
    oclass->get_property = ufo_node_get_property;
    oclass->finalize = ufo_node_finalize;
    oclass->dispose = ufo_node_dispose;

    properties[PROP_GRAPH] =
        g_param_spec_object ("graph",
                             "Graph the node is member of",
                             "Graph the node is member of",
                             UFO_TYPE_GRAPH,
                             G_PARAM_READWRITE
        );

    g_object_class_install_properties (oclass,
                                       N_PROPERTIES,
                                       properties);

    g_type_class_add_private(klass, sizeof(UfoNodePrivate));
}

static void
ufo_node_init (UfoNode *self)
{
    UfoNodePrivate *priv;
    self->priv = priv = UFO_NODE_GET_PRIVATE (self);

    priv->copied_from = NULL;
    priv->index = 0;
    priv->total = 1;
    priv->graph = NULL;
    priv->label = NULL;
}
