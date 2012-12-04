#include "ufo-node.h"

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
