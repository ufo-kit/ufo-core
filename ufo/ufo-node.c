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

/**
 * SECTION:ufo-node
 * @Short_description: Generic node type
 * @Title: UfoNode
 */

G_DEFINE_TYPE (UfoNode, ufo_node, G_TYPE_OBJECT)

#define UFO_NODE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_NODE, UfoNodePrivate))

static UfoNode *ufo_node_copy_real (UfoNode *node, GError **error);

struct _UfoNodePrivate {
    UfoNode  *orig;
    guint     total;
    guint     index;
    gpointer  label;
};

enum {
    PROP_0,
    N_PROPERTIES
};

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
        if (props[i]->flags & G_PARAM_WRITABLE) {
            GValue value = {0};

            g_value_init (&value, props[i]->value_type);
            g_object_get_property (src, props[i]->name, &value);
            if(UFO_IS_NODE_CLASS(&(props[i]->value_type))) {
                g_value_set_object(&value, ufo_node_copy_real(UFO_NODE(g_value_get_object(&value)), NULL));
            }

            g_object_set_property (dst, props[i]->name, &value);
        }
    }

    g_free (props);
}

static UfoNode *
ufo_node_copy_real (UfoNode *node,
                    GError **error)
{
    UfoNode *orig;
    UfoNode *copy;

    copy = UFO_NODE (g_object_new (G_OBJECT_TYPE (node), NULL));
    orig = node->priv->orig;

    copy_properties (G_OBJECT (copy), G_OBJECT (orig));

    copy->priv->orig = orig;
    copy->priv->label = orig->priv->label;
    copy->priv->index = orig->priv->total;
    orig->priv->total++;

    return copy;
}

static gboolean
ufo_node_equal_real (UfoNode *n1,
                     UfoNode *n2)
{
    g_return_val_if_fail (UFO_IS_NODE (n1) && UFO_IS_NODE (n2), FALSE);

    /* FIXME: When done we should just check if the types match */
    return n1 == n2;
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
    return UFO_NODE_GET_CLASS (node)->copy (node, error);
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
    return node->priv->orig->priv->total;
}

gboolean
ufo_node_equal (UfoNode *n1,
                UfoNode *n2)
{
    return UFO_NODE_GET_CLASS (n1)->equal (n1, n2);
}

static void
ufo_node_class_init (UfoNodeClass *klass)
{
    klass->copy = ufo_node_copy_real;
    klass->equal = ufo_node_equal_real;

    g_type_class_add_private (klass, sizeof(UfoNodePrivate));
}

static void
ufo_node_init (UfoNode *self)
{
    UfoNodePrivate *priv;
    self->priv = priv = UFO_NODE_GET_PRIVATE (self);
    priv->orig = self;
    priv->label = NULL;
    priv->total = 1;
    priv->index = 0;
}
