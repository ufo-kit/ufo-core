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
#include <string.h>
#include <ufo/ufo-resources.h>
#include <ufo/ufo-gpu-node.h>

G_DEFINE_TYPE (UfoGpuNode, ufo_gpu_node, UFO_TYPE_NODE)

#define UFO_GPU_NODE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GPU_NODE, UfoGpuNodePrivate))


struct _UfoGpuNodePrivate {
    cl_context context;
    cl_device_id device;
    cl_command_queue cmd_queue;
};

UfoNode *
ufo_gpu_node_new (gpointer context, gpointer device)
{
    UfoGpuNode *node;
    cl_int errcode;
    cl_command_queue_properties queue_properties;

    g_return_val_if_fail (context != NULL && device != NULL, NULL);

    queue_properties = CL_QUEUE_PROFILING_ENABLE;

    node = UFO_GPU_NODE (g_object_new (UFO_TYPE_GPU_NODE, NULL));
    node->priv->context = context;
    node->priv->device = device;
    node->priv->cmd_queue = clCreateCommandQueue (context, device, queue_properties, &errcode);

    UFO_RESOURCES_CHECK_CLERR (errcode);
    UFO_RESOURCES_CHECK_CLERR (clRetainContext (context));

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

/**
 * ufo_gpu_node_get_info:
 * @node: A #UfoGpuNodeInfo
 * @info: Information to be queried
 *
 * Return information about the associated OpenCL device.
 *
 * Returns: (transfer full): Information about @info.
 */
GValue *
ufo_gpu_node_get_info (UfoGpuNode *node,
                       UfoGpuNodeInfo info)
{
    UfoGpuNodePrivate *priv;
    GValue *value;
    cl_ulong ulong_value;

    priv = UFO_GPU_NODE_GET_PRIVATE (node);
    value = g_new0 (GValue, 1);
    memset (value, 0, sizeof (GValue));

    g_value_init (value, G_TYPE_ULONG);

    switch (info) {
        case UFO_GPU_NODE_INFO_GLOBAL_MEM_SIZE:
            UFO_RESOURCES_CHECK_CLERR (clGetDeviceInfo (priv->device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof (cl_ulong), &ulong_value, NULL));
            break;

        case UFO_GPU_NODE_INFO_MAX_MEM_ALLOC_SIZE:
            UFO_RESOURCES_CHECK_CLERR (clGetDeviceInfo (priv->device, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof (cl_ulong), &ulong_value, NULL));
            break;

        case UFO_GPU_NODE_INFO_LOCAL_MEM_SIZE:
            UFO_RESOURCES_CHECK_CLERR (clGetDeviceInfo (priv->device, CL_DEVICE_LOCAL_MEM_SIZE, sizeof (cl_ulong), &ulong_value, NULL));
            break;
    }

    g_value_set_ulong (value, ulong_value);
    return value;
}

static UfoNode *
ufo_gpu_node_copy_real (UfoNode *node,
                        GError **error)
{
    UfoGpuNode *orig;

    orig = UFO_GPU_NODE (node);
    return ufo_gpu_node_new (orig->priv->context, orig->priv->device);
}

static gboolean
ufo_gpu_node_equal_real (UfoNode *n1,
                         UfoNode *n2)
{
    g_return_val_if_fail (UFO_IS_GPU_NODE (n1) && UFO_IS_GPU_NODE (n2), FALSE);
    return UFO_GPU_NODE (n1)->priv->cmd_queue == UFO_GPU_NODE (n2)->priv->cmd_queue;
}

static void
ufo_gpu_node_finalize (GObject *object)
{
    UfoGpuNodePrivate *priv;

    priv = UFO_GPU_NODE_GET_PRIVATE (object);

    if (priv->cmd_queue != NULL) {
        g_debug ("Release cmd_queue=%p", (gpointer) priv->cmd_queue);
        UFO_RESOURCES_CHECK_CLERR (clReleaseCommandQueue (priv->cmd_queue));
        priv->cmd_queue = NULL;

        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
    }

    G_OBJECT_CLASS (ufo_gpu_node_parent_class)->finalize (object);
}

static void
ufo_gpu_node_class_init (UfoGpuNodeClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);
    UfoNodeClass *node_class = UFO_NODE_CLASS (klass);

    oclass->finalize = ufo_gpu_node_finalize;
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
