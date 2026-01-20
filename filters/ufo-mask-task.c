/*
 * Copyright (C) 2011-2015 Karlsruhe Institute of Technology
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

#include "ufo-mask-task.h"


struct _UfoMaskTaskPrivate {
    cl_kernel kernel;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoMaskTask, ufo_mask_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_MASK_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_MASK_TASK, UfoMaskTaskPrivate))

UfoNode *
ufo_mask_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_MASK_TASK, NULL));
}

static void
ufo_mask_task_setup (UfoTask *task,
                     UfoResources *resources,
                     GError **error)
{
    UfoMaskTaskPrivate *priv;

    priv = UFO_MASK_TASK_GET_PRIVATE (task);
    priv->kernel = ufo_resources_get_kernel (resources, "mask.cl", "circular", NULL, error);

    if (priv->kernel != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
}

static void
ufo_mask_task_get_requisition (UfoTask *task,
                               UfoBuffer **inputs,
                               UfoRequisition *requisition,
                               GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_mask_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_mask_task_get_num_dimensions (UfoTask *task,
                                  guint input)
{
    return 2;
}

static UfoTaskMode
ufo_mask_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_mask_task_process (UfoTask *task,
                       UfoBuffer **inputs,
                       UfoBuffer *output,
                       UfoRequisition *requisition)
{
    UfoMaskTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    cl_mem in_mem;
    cl_mem out_mem;

    priv = UFO_MASK_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &out_mem));

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, requisition->dims, NULL);

    return TRUE;
}

static void
ufo_mask_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_mask_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_mask_task_setup;
    iface->get_num_inputs = ufo_mask_task_get_num_inputs;
    iface->get_num_dimensions = ufo_mask_task_get_num_dimensions;
    iface->get_mode = ufo_mask_task_get_mode;
    iface->get_requisition = ufo_mask_task_get_requisition;
    iface->process = ufo_mask_task_process;
}

static void
ufo_mask_task_class_init (UfoMaskTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = ufo_mask_task_finalize;

    g_type_class_add_private (oclass, sizeof(UfoMaskTaskPrivate));
}

static void
ufo_mask_task_init(UfoMaskTask *self)
{
    self->priv = UFO_MASK_TASK_GET_PRIVATE(self);
    self->priv->kernel = NULL;
}
