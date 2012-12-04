#define _GNU_SOURCE
#include <sched.h>
#include "ufo-task-node.h"

G_DEFINE_TYPE (UfoTaskNode, ufo_task_node, UFO_TYPE_NODE)

#define UFO_TASK_NODE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_TASK_NODE, UfoTaskNodePrivate))


struct _UfoTaskNodePrivate {
    UfoSendPattern   pattern;
    UfoNode         *proc_node;
    UfoGroup        *out_group;
    GList           *in_groups[16];
    GList           *current[16];
};

void
ufo_task_node_set_send_pattern (UfoTaskNode *node,
                                UfoSendPattern pattern)
{
    g_return_if_fail (UFO_IS_TASK_NODE (node));
    node->priv->pattern = pattern;
}

UfoSendPattern
ufo_task_node_get_send_pattern (UfoTaskNode *node)
{
    g_return_val_if_fail (UFO_IS_TASK_NODE (node), 0);
    return node->priv->pattern;
}

void
ufo_task_node_set_out_group (UfoTaskNode *node,
                             UfoGroup *group)
{
    g_return_if_fail (UFO_IS_TASK_NODE (node));
    node->priv->out_group = group;
}

UfoGroup *
ufo_task_node_get_out_group (UfoTaskNode *node)
{
    g_return_val_if_fail (UFO_IS_TASK_NODE (node), NULL);
    return node->priv->out_group;
}

void
ufo_task_node_add_in_group (UfoTaskNode *node,
                            guint pos,
                            UfoGroup *group)
{
    g_return_if_fail (UFO_IS_TASK_NODE (node));
    /* TODO: check out-of-bounds condition */
    node->priv->in_groups[pos] = g_list_prepend (node->priv->in_groups[pos], group);
    node->priv->current[pos] = node->priv->in_groups[pos];
}

UfoGroup *
ufo_task_node_get_current_in_group (UfoTaskNode *node,
                                    guint pos)
{
    g_return_val_if_fail (UFO_IS_TASK_NODE (node), NULL);
    return UFO_GROUP (node->priv->current[pos]->data);
}

void
ufo_task_node_switch_in_group (UfoTaskNode *node,
                               guint pos)
{
    UfoTaskNodePrivate *priv;

    g_return_if_fail (UFO_IS_TASK_NODE (node));
    priv = node->priv;
    priv->current[pos] = g_list_next (priv->current[pos]);

    if (priv->current[pos] == NULL)
        priv->current[pos] = g_list_first (priv->in_groups[pos]);
}

void
ufo_task_node_set_proc_node (UfoTaskNode *task_node,
                             UfoNode *proc_node)
{
    g_return_if_fail (UFO_IS_TASK_NODE (task_node) && UFO_IS_NODE (proc_node));
    task_node->priv->proc_node = proc_node;
}

UfoNode *
ufo_task_node_get_proc_node (UfoTaskNode *node)
{
    g_return_val_if_fail (UFO_IS_TASK_NODE (node), NULL);
    return node->priv->proc_node;
}

static void
ufo_task_node_class_init (UfoTaskNodeClass *klass)
{
    g_type_class_add_private (klass, sizeof(UfoTaskNodePrivate));
}

static void
ufo_task_node_init (UfoTaskNode *self)
{
    self->priv = UFO_TASK_NODE_GET_PRIVATE (self);
    self->priv->pattern = UFO_SEND_BROADCAST;
    self->priv->proc_node = NULL;
    self->priv->out_group = NULL;

    for (guint i = 0; i < 16; i++) {
        self->priv->in_groups[i] = NULL;
        self->priv->current[i] = NULL;
    }
}
