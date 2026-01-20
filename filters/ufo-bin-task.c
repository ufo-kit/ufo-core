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

#include "ufo-bin-task.h"


struct _UfoBinTaskPrivate {
    guint size;
    cl_kernel kernel_2d;
    cl_kernel kernel_3d;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoBinTask, ufo_bin_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_BIN_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BIN_TASK, UfoBinTaskPrivate))

enum {
    PROP_0,
    PROP_SIZE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_bin_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_BIN_TASK, NULL));
}

static void
ufo_bin_task_setup (UfoTask *task,
                    UfoResources *resources,
                    GError **error)
{
    UfoBinTaskPrivate *priv;

    priv = UFO_BIN_TASK_GET_PRIVATE (task);

    priv->kernel_2d = ufo_resources_get_kernel (resources, "bin.cl", "binning_2d", NULL, error);

    if (priv->kernel_2d != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel_2d), error);

    priv->kernel_3d = ufo_resources_get_kernel (resources, "bin.cl", "binning_3d", NULL, error);

    if (priv->kernel_3d != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel_3d), error);
}

static void
ufo_bin_task_get_requisition (UfoTask *task,
                              UfoBuffer **inputs,
                              UfoRequisition *requisition,
                              GError **error)
{
    UfoBinTaskPrivate *priv;
    UfoRequisition in_req;

    priv = UFO_BIN_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], &in_req);

    requisition->n_dims = in_req.n_dims;
    requisition->dims[0] = in_req.dims[0] / priv->size;
    requisition->dims[1] = in_req.dims[1] / priv->size;
    requisition->dims[2] = in_req.dims[2] / priv->size;
}

static guint
ufo_bin_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_bin_task_get_num_dimensions (UfoTask *task,
                                 guint input)
{
    return 2;
}

static UfoTaskMode
ufo_bin_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_bin_task_process (UfoTask *task,
                      UfoBuffer **inputs,
                      UfoBuffer *output,
                      UfoRequisition *requisition)
{
    UfoBinTaskPrivate *priv;
    UfoRequisition in_req;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    cl_mem in_mem;
    cl_mem out_mem;
    cl_kernel kernel;

    priv = UFO_BIN_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);

    in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    ufo_buffer_get_requisition (inputs[0], &in_req);
    kernel = in_req.n_dims == 2 ? priv->kernel_2d : priv->kernel_3d;

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 1, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 2, sizeof (guint), &priv->size));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 3, sizeof (guint), &in_req.dims[0]));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 4, sizeof (guint), &in_req.dims[1]));

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, kernel, in_req.n_dims, requisition->dims, NULL);

    return TRUE;
}

static void
ufo_bin_task_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
    UfoBinTaskPrivate *priv = UFO_BIN_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_SIZE:
            priv->size = g_value_get_uint (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_bin_task_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
    UfoBinTaskPrivate *priv = UFO_BIN_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_SIZE:
            g_value_set_uint (value, priv->size);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_bin_task_finalize (GObject *object)
{
    UfoBinTaskPrivate *priv;
    
    priv = UFO_BIN_TASK_GET_PRIVATE (object);

    if (priv->kernel_2d) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel_2d));
        priv->kernel_2d = NULL;
    }

    if (priv->kernel_3d) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel_3d));
        priv->kernel_3d = NULL;
    }

    G_OBJECT_CLASS (ufo_bin_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_bin_task_setup;
    iface->get_num_inputs = ufo_bin_task_get_num_inputs;
    iface->get_num_dimensions = ufo_bin_task_get_num_dimensions;
    iface->get_mode = ufo_bin_task_get_mode;
    iface->get_requisition = ufo_bin_task_get_requisition;
    iface->process = ufo_bin_task_process;
}

static void
ufo_bin_task_class_init (UfoBinTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_bin_task_set_property;
    oclass->get_property = ufo_bin_task_get_property;
    oclass->finalize = ufo_bin_task_finalize;

    properties[PROP_SIZE] =
        g_param_spec_uint ("size",
            "Number of pixels to bin",
            "Number of pixels to bin",
            2, 16, 2,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoBinTaskPrivate));
}

static void
ufo_bin_task_init(UfoBinTask *self)
{
    self->priv = UFO_BIN_TASK_GET_PRIVATE(self);
    self->priv->size = 2;
}
