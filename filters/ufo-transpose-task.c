/*
 * Copyright (C) 2011-2014 Karlsruhe Institute of Technology
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
#include "config.h"
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include "ufo-transpose-task.h"

#define PIXELS_PER_THREAD 4

struct _UfoTransposeTaskPrivate {
    cl_kernel kernel;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoTransposeTask, ufo_transpose_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_TRANSPOSE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_TRANSPOSE_TASK, UfoTransposeTaskPrivate))

UfoNode *
ufo_transpose_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_TRANSPOSE_TASK, NULL));
}

static void
ufo_transpose_task_setup (UfoTask *task,
                          UfoResources *resources,
                          GError **error)
{
    UfoTransposeTaskPrivate *priv = UFO_TRANSPOSE_TASK_GET_PRIVATE (task);
    priv->kernel = ufo_resources_get_kernel (resources, "transpose.cl", "transpose_shared", NULL, error);

    if (priv->kernel)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
}

static void
ufo_transpose_task_get_requisition (UfoTask *task,
                                    UfoBuffer **inputs,
                                    UfoRequisition *requisition,
                                    GError **error)
{
    UfoRequisition in_req;

    ufo_buffer_get_requisition (inputs[0], &in_req);

    requisition->n_dims = 2;
    requisition->dims[0] = in_req.dims[1];
    requisition->dims[1] = in_req.dims[0];
}

static guint
ufo_transpose_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_transpose_task_get_num_dimensions (UfoTask *task,
                                       guint input)
{
    g_return_val_if_fail (input == 0, 0);

    return 2;
}

static UfoTaskMode
ufo_transpose_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_transpose_task_process (UfoTask *task,
                            UfoBuffer **inputs,
                            UfoBuffer *output,
                            UfoRequisition *requisition)
{
    UfoTransposeTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    UfoRequisition in_req;
    cl_command_queue cmd_queue;
    cl_mem in_mem;
    cl_mem out_mem;
    cl_int width, height;
    GValue *work_group_size_gvalue;
    size_t work_group_size;
    gsize global_size[2];
    gsize local_size[2] = {32, 32 / PIXELS_PER_THREAD};

    priv = UFO_TRANSPOSE_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    ufo_buffer_get_requisition (inputs[0], &in_req);
    width = (cl_int) in_req.dims[0];
    height = (cl_int) in_req.dims[1];

    work_group_size_gvalue = ufo_gpu_node_get_info (node, UFO_GPU_NODE_INFO_MAX_WORK_GROUP_SIZE);
    work_group_size = g_value_get_ulong (work_group_size_gvalue);
    g_value_unset (work_group_size_gvalue);
    while (local_size[0] * local_size[1] > work_group_size) {
        local_size[0] /= 2;
        local_size[1] /= 2;
    }
    global_size[0] = ((in_req.dims[0] - 1) / local_size[0] + 1) * local_size[0];
    global_size[1] = ((in_req.dims[1] - 1) / local_size[1] / PIXELS_PER_THREAD + 1) * local_size[1];
    g_debug ("Image size: %lu x %lu", in_req.dims[0], in_req.dims[1]);
    g_debug ("Transpose global work group size: %lu x %lu", global_size[0], global_size[1]);
    g_debug ("Transpose local work group size: %lu x %lu", local_size[0], local_size[1]);

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &out_mem));
    /* (local_size[0] + 1) to prevent shared memory bank conflicts */
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2,
                               (local_size[0] + 1) * local_size[1] * PIXELS_PER_THREAD * sizeof (cl_float), NULL));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 3, sizeof (cl_int), &width));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 4, sizeof (cl_int), &height));
    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, global_size, local_size);

    return TRUE;
}

static void
ufo_transpose_task_finalize (GObject *object)
{
    UfoTransposeTaskPrivate *priv = UFO_TRANSPOSE_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }

    G_OBJECT_CLASS (ufo_transpose_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_transpose_task_setup;
    iface->get_num_inputs = ufo_transpose_task_get_num_inputs;
    iface->get_num_dimensions = ufo_transpose_task_get_num_dimensions;
    iface->get_mode = ufo_transpose_task_get_mode;
    iface->get_requisition = ufo_transpose_task_get_requisition;
    iface->process = ufo_transpose_task_process;
}

static void
ufo_transpose_task_class_init (UfoTransposeTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = ufo_transpose_task_finalize;
    g_type_class_add_private (oclass, sizeof(UfoTransposeTaskPrivate));
}

static void
ufo_transpose_task_init(UfoTransposeTask *self)
{
}
