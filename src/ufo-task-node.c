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

#define _GNU_SOURCE
#include <sched.h>
#include <ufo-task-node.h>

G_DEFINE_TYPE (UfoTaskNode, ufo_task_node, UFO_TYPE_NODE)

#define UFO_TASK_NODE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_TASK_NODE, UfoTaskNodePrivate))


struct _UfoTaskNodePrivate {
    gchar           *plugin;
    gchar           *unique;
    UfoSendPattern   pattern;
    UfoNode         *proc_node;
    UfoGroup        *out_group;
    GList           *in_groups[16];
    GList           *current[16];
};

void
ufo_task_node_set_plugin_name (UfoTaskNode *task_node,
                               const gchar *name)
{
    UfoTaskNodePrivate *priv;

    g_return_if_fail (UFO_IS_TASK_NODE (task_node));
    priv = task_node->priv;

    g_free (priv->plugin);
    priv->plugin = g_strdup (name);

    g_free (priv->unique);
    priv->unique = g_strdup_printf ("%s-%p", name, (gpointer) task_node);
}

const gchar *
ufo_task_node_get_plugin_name (UfoTaskNode *task_node)
{
    g_return_val_if_fail (UFO_IS_TASK_NODE (task_node), NULL);
    return task_node->priv->plugin;
}

const gchar *
ufo_task_node_get_unique_name (UfoTaskNode *task_node)
{
    g_return_val_if_fail (UFO_IS_TASK_NODE (task_node), NULL);
    return task_node->priv->unique;
}

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
ufo_task_node_dispose (GObject *object)
{
    G_OBJECT_CLASS (ufo_task_node_parent_class)->dispose (object);
}

static void
ufo_task_node_finalize (GObject *object)
{
    UfoTaskNodePrivate *priv;

    priv = UFO_TASK_NODE_GET_PRIVATE (object);
    g_free (priv->plugin);
    g_free (priv->unique);

    G_OBJECT_CLASS (ufo_task_node_parent_class)->finalize (object);
}

static void
ufo_task_node_class_init (UfoTaskNodeClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);
    oclass->dispose = ufo_task_node_dispose;
    oclass->finalize = ufo_task_node_finalize;

    g_type_class_add_private (klass, sizeof(UfoTaskNodePrivate));
}

static void
ufo_task_node_init (UfoTaskNode *self)
{
    self->priv = UFO_TASK_NODE_GET_PRIVATE (self);
    self->priv->plugin = NULL;
    self->priv->unique = NULL;
    self->priv->pattern = UFO_SEND_BROADCAST;
    self->priv->proc_node = NULL;
    self->priv->out_group = NULL;

    for (guint i = 0; i < 16; i++) {
        self->priv->in_groups[i] = NULL;
        self->priv->current[i] = NULL;
    }
}
