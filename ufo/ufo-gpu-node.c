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

#include <CL/cl.h>
#include <ufo/ufo-gpu-node.h>

G_DEFINE_TYPE (UfoGpuNode, ufo_gpu_node, UFO_TYPE_NODE)

#define UFO_GPU_NODE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GPU_NODE, UfoGpuNodePrivate))


struct _UfoGpuNodePrivate {
    gpointer cmd_queue;
};

UfoNode *
ufo_gpu_node_new (gpointer cmd_queue)
{
    UfoGpuNode *node;

    g_return_val_if_fail (cmd_queue != NULL, NULL);
    node = UFO_GPU_NODE (g_object_new (UFO_TYPE_GPU_NODE, NULL));
    node->priv->cmd_queue = cmd_queue;
    clRetainCommandQueue (cmd_queue);

    return UFO_NODE (node);
}

/**
 * ufo_gpu_node_get_cmd_queue:
 * @node: A #UfoGpuNode
 *
 * Get command queue associated with @node.
 *
 * Returns: (transfer none): A cl_command_queue object for @node.
 */
gpointer
ufo_gpu_node_get_cmd_queue (UfoGpuNode *node)
{
    g_return_val_if_fail (UFO_IS_GPU_NODE (node), NULL);
    return node->priv->cmd_queue;
}

static UfoNode *
ufo_gpu_node_copy_real (UfoNode *node,
                        GError **error)
{
    return UFO_NODE (ufo_gpu_node_new (UFO_GPU_NODE (node)->priv->cmd_queue));
}

static gboolean
ufo_gpu_node_equal_real (UfoNode *n1,
                         UfoNode *n2)
{
    g_return_val_if_fail (UFO_IS_GPU_NODE (n1) && UFO_IS_GPU_NODE (n2), FALSE);
    return UFO_GPU_NODE (n1)->priv->cmd_queue == UFO_GPU_NODE (n2)->priv->cmd_queue;
}

static void
ufo_gpu_node_dispose (GObject *object)
{
    UfoGpuNodePrivate *priv;

    priv = UFO_GPU_NODE_GET_PRIVATE (object);

    if (priv->cmd_queue != NULL) {
        clReleaseCommandQueue (priv->cmd_queue);
        priv->cmd_queue = NULL;
    }

    G_OBJECT_CLASS (ufo_gpu_node_parent_class)->dispose (object);
}

static void
ufo_gpu_node_class_init (UfoGpuNodeClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);
    UfoNodeClass *node_class = UFO_NODE_CLASS (klass);

    oclass->dispose = ufo_gpu_node_dispose;
    node_class->copy = ufo_gpu_node_copy_real;
    node_class->equal = ufo_gpu_node_equal_real;

    g_type_class_add_private (klass, sizeof (UfoGpuNodePrivate));
}

static void
ufo_gpu_node_init (UfoGpuNode *self)
{
    UfoGpuNodePrivate *priv;
    self->priv = priv = UFO_GPU_NODE_GET_PRIVATE (self);
    priv->cmd_queue = NULL;
}
