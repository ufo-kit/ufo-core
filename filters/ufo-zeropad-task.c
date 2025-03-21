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

#include <stdio.h>
#include <math.h>
#include "ufo-zeropad-task.h"

/**
 * SECTION:ufo-zeropad-task
 * @Short_description: Add zeros in the center of sinogram
 * @Title: zeropad
 *
 * Add zeros in the center of sinogram using #UfoZeropadTask:oversampling
 * to manage the amount of zeros which will be added.
 *
 */

struct _UfoZeropadTaskPrivate {
    UfoResources *resources;
    cl_kernel zeropad_kernel;
    guint oversampling;
    gfloat center_rot;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoZeropadTask, ufo_zeropad_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_ZEROPAD_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_ZEROPAD_TASK, UfoZeropadTaskPrivate))

enum {
    PROP_0,
    PROP_OVERSAMPLING,
    PROP_CENTER_ROT,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_zeropad_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_ZEROPAD_TASK, NULL));
}

static void
ufo_zeropad_task_setup (UfoTask *task,
                            UfoResources *resources,
                            GError **error)
{
    UfoZeropadTaskPrivate *priv = UFO_ZEROPAD_TASK_GET_PRIVATE (task);
    priv->resources = resources;
    priv->zeropad_kernel = ufo_resources_get_kernel (resources, "zeropad.cl", "zeropadding_kernel", NULL, error);
}

static void
ufo_zeropad_task_get_requisition (UfoTask *task,
                                  UfoBuffer **inputs,
                                  UfoRequisition *requisition,
                                  GError **error)
{
    UfoZeropadTaskPrivate *priv = UFO_ZEROPAD_TASK_GET_PRIVATE (task);

    UfoRequisition input_requisition;
    ufo_buffer_get_requisition (inputs[0], &input_requisition);

    requisition->n_dims = input_requisition.n_dims;
    requisition->dims[0] = (gsize)(2 * pow(2, ceil(log2f((gfloat)input_requisition.dims[0]))) * priv->oversampling);
    requisition->dims[1] = input_requisition.dims[1];
}

static guint
ufo_zeropad_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_zeropad_task_get_num_dimensions (UfoTask *task,
                               guint input)
{
    g_return_val_if_fail (input == 0, 0);
    return 2;
}

static UfoTaskMode
ufo_zeropad_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}


static gboolean
ufo_zeropad_task_process (UfoTask *task,
                              UfoBuffer **inputs,
                              UfoBuffer *output,
                              UfoRequisition *requisition)
{
    UfoZeropadTaskPrivate *priv;
    UfoRequisition input_requisition;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    cl_mem in_mem, out_mem;
    cl_int xdim, offset;

    priv = UFO_ZEROPAD_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);

    ufo_buffer_get_requisition (inputs[0], &input_requisition);

    xdim = (cl_int) input_requisition.dims[0];
    offset = (priv->center_rot != -1) ? (cl_int ) -(xdim / 2.0f - priv->center_rot) : 0;

    in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->zeropad_kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->zeropad_kernel, 1, sizeof (cl_int), &offset));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->zeropad_kernel, 2, sizeof (cl_int), &xdim));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->zeropad_kernel, 3, sizeof (cl_mem), &out_mem));

    /* execution */
    size_t working_dims[] = { requisition->dims[0] / 2, requisition->dims[1] };
    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, priv->zeropad_kernel, requisition->n_dims, working_dims, NULL);
    
    return TRUE;
}

static void
ufo_zeropad_task_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
    UfoZeropadTaskPrivate *priv = UFO_ZEROPAD_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_OVERSAMPLING:
            priv->oversampling = g_value_get_uint (value);
            break;
        case PROP_CENTER_ROT:
            priv->center_rot = g_value_get_float (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_zeropad_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoZeropadTaskPrivate *priv = UFO_ZEROPAD_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_OVERSAMPLING:
            g_value_set_uint (value, priv->oversampling);
            break;
        case PROP_CENTER_ROT:
            g_value_set_float (value, priv->center_rot);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_zeropad_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_zeropad_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_zeropad_task_setup;
    iface->get_num_inputs = ufo_zeropad_task_get_num_inputs;
    iface->get_num_dimensions = ufo_zeropad_task_get_num_dimensions;
    iface->get_mode = ufo_zeropad_task_get_mode;
    iface->get_requisition = ufo_zeropad_task_get_requisition;
    iface->process = ufo_zeropad_task_process;
}

static void
ufo_zeropad_task_class_init (UfoZeropadTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_zeropad_task_set_property;
    gobject_class->get_property = ufo_zeropad_task_get_property;
    gobject_class->finalize = ufo_zeropad_task_finalize;

    properties[PROP_OVERSAMPLING] =
        g_param_spec_uint ("oversampling",
            "Oversamling",
            "Oversamling coefficient",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    properties[PROP_CENTER_ROT] =
        g_param_spec_float ("center-of-rotation",
            "Center of rotation",
            "Center of rotation of specimen",
            -1, G_MAXFLOAT, -1,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoZeropadTaskPrivate));
}

static void
ufo_zeropad_task_init(UfoZeropadTask *self)
{
    self->priv = UFO_ZEROPAD_TASK_GET_PRIVATE(self);
    self->priv->oversampling = 1;
    self->priv->center_rot = -1;
}
