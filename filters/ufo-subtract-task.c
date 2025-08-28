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
#include "config.h"

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "ufo-subtract-task.h"

/**
 * SECTION:ufo-subtract-task
 * @Short_description: Write TIFF files
 * @Title: subtract
 *
 */

struct _UfoSubtractTaskPrivate {
    cl_kernel kernel;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoSubtractTask, ufo_subtract_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_SUBTRACT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_SUBTRACT_TASK, UfoSubtractTaskPrivate))

enum {
    PROP_0,
    N_PROPERTIES
};

UfoNode *
ufo_subtract_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_SUBTRACT_TASK, NULL));
}

static void
ufo_subtract_task_setup (UfoTask *task,
                         UfoResources *resources,
                         GError **error)
{
    UfoSubtractTaskPrivate *priv;

    priv = UFO_SUBTRACT_TASK_GET_PRIVATE (task);
    priv->kernel = ufo_resources_get_kernel (resources, "arithmetics.cl", "subtract", NULL, error);

    if (priv->kernel != NULL)
        UFO_RESOURCES_CHECK_AND_SET (clRetainKernel (priv->kernel), error);
}

static void
ufo_subtract_task_get_requisition (UfoTask *task,
                                   UfoBuffer **inputs,
                                   UfoRequisition *requisition,
                                   GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_subtract_task_get_num_inputs (UfoTask *task)
{
    return 2;
}

static guint
ufo_subtract_task_get_num_dimensions (UfoTask *task, guint input)
{
    g_return_val_if_fail (input <= 1, 0);
    return 2;
}

static UfoTaskMode
ufo_subtract_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_subtract_task_process (UfoTask *task,
                           UfoBuffer **inputs,
                           UfoBuffer *output,
                           UfoRequisition *requisition)
{
    UfoSubtractTaskPrivate *priv;
    UfoGpuNode *node;
    cl_command_queue cmd_queue;
    cl_mem a_mem;
    cl_mem b_mem;
    cl_mem y_mem;
    gsize work_size;

    priv = UFO_SUBTRACT_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    a_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
    b_mem = ufo_buffer_get_device_array (inputs[1], cmd_queue);
    y_mem = ufo_buffer_get_device_array (output, cmd_queue);

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &a_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &b_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (cl_mem), &y_mem));

    /* Launch the kernel over 1D grid */
    work_size = requisition->dims[0] * requisition->dims[1];
    UFO_RESOURCES_CHECK_CLERR (clEnqueueNDRangeKernel (cmd_queue,
                                                       priv->kernel,
                                                       1, NULL, &work_size, NULL,
                                                       0, NULL, NULL));

    return TRUE;
}

static void
ufo_subtract_task_finalize (GObject *object)
{
    UfoSubtractTaskPrivate *priv;

    priv = UFO_SUBTRACT_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }

    G_OBJECT_CLASS (ufo_subtract_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_subtract_task_setup;
    iface->get_num_inputs = ufo_subtract_task_get_num_inputs;
    iface->get_num_dimensions = ufo_subtract_task_get_num_dimensions;
    iface->get_mode = ufo_subtract_task_get_mode;
    iface->get_requisition = ufo_subtract_task_get_requisition;
    iface->process = ufo_subtract_task_process;
}

static void
ufo_subtract_task_class_init (UfoSubtractTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = ufo_subtract_task_finalize;

    g_type_class_add_private (gobject_class, sizeof(UfoSubtractTaskPrivate));
}

static void
ufo_subtract_task_init(UfoSubtractTask *self)
{
    self->priv = UFO_SUBTRACT_TASK_GET_PRIVATE(self);
    self->priv->kernel = NULL;
}
