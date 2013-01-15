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

#include <ufo-node.h>

/**
 * SECTION:ufo-node
 * @Short_description: Generic node that can be connected in a #UfoGraph
 * @Title: UfoNode
 */

G_DEFINE_TYPE (UfoNode, ufo_node, G_TYPE_OBJECT)

#define UFO_NODE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_NODE, UfoNodePrivate))

struct _UfoNodePrivate {
    UfoNode  *copied_from;
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

static UfoNode *
ufo_node_copy_real (UfoNode *node,
                    GError **error)
{
    return ufo_node_new (node->priv->label);
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

/**
 * ufo_node_copy:
 * @node: A #UfoNode
 * @error: Location for an error
 *
 * Get a copy of @node. How "deep" the copy is, depends on the inherited
 * implementation of @node.
 *
 * Returns: (transfer full): Copy of @node.
 */
UfoNode *
ufo_node_copy (UfoNode *node,
               GError **error)
{
    UfoNode *offspring;

    offspring = UFO_NODE_GET_CLASS (node)->copy (node, error);
    offspring->priv->copied_from = node;
    return offspring;
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

    g_type_class_add_private(klass, sizeof(UfoNodePrivate));
}

static void
ufo_node_init (UfoNode *self)
{
    UfoNodePrivate *priv;
    self->priv = priv = UFO_NODE_GET_PRIVATE (self);

    priv->copied_from = NULL;
    priv->label = NULL;
}
