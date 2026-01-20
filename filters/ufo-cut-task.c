/*
 * Copyright (C) 2017 Karlsruhe Institute of Technology
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

#include "ufo-cut-task.h"


struct _UfoCutTaskPrivate {
    guint width;
    cl_kernel kernel;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoCutTask, ufo_cut_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_CUT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CUT_TASK, UfoCutTaskPrivate))

enum {
    PROP_0,
    PROP_WIDTH,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_cut_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_CUT_TASK, NULL));
}

static void
ufo_cut_task_setup (UfoTask *task,
                    UfoResources *resources,
                    GError **error)
{
    UfoCutTaskPrivate *priv;

    priv = UFO_CUT_TASK_GET_PRIVATE (task);
    priv->kernel = ufo_resources_get_kernel (resources, "cut.cl", "cut", NULL, error);

    if (priv->kernel != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
}

static void
ufo_cut_task_get_requisition (UfoTask *task,
                              UfoBuffer **inputs,
                              UfoRequisition *requisition,
                              GError **error)
{
    UfoCutTaskPrivate *priv;
    UfoRequisition in_req;
    gsize width;

    priv = UFO_CUT_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], &in_req);

    if (priv->width >= in_req.dims[0]) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_GET_REQUISITION,
                     "Cut width %u larger than input width %zu", priv->width, in_req.dims[0]);
        return;
    }
    else {
        width = in_req.dims[0] - priv->width;
    }

    requisition->n_dims = 2;
    requisition->dims[0] = width;
    requisition->dims[1] = in_req.dims[1];
}

static guint
ufo_cut_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_cut_task_get_num_dimensions (UfoTask *task,
                                 guint input)
{
    return 2;
}

static UfoTaskMode
ufo_cut_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_cut_task_process (UfoTask *task,
                      UfoBuffer **inputs,
                      UfoBuffer *output,
                      UfoRequisition *requisition)
{
    UfoCutTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    UfoRequisition in_req;
    cl_command_queue cmd_queue;
    cl_mem in_mem;
    cl_mem out_mem;
    guint width;

    priv = UFO_CUT_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    ufo_buffer_get_requisition (inputs[0], &in_req);
    width = (guint) in_req.dims[0];

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (guint), &width));

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, requisition->dims, NULL);

    return TRUE;
}

static void
ufo_cut_task_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
    UfoCutTaskPrivate *priv = UFO_CUT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_WIDTH:
            priv->width = g_value_get_uint (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_cut_task_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
    UfoCutTaskPrivate *priv = UFO_CUT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_WIDTH:
            g_value_set_uint (value, priv->width);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_cut_task_finalize (GObject *object)
{
    UfoCutTaskPrivate *priv;

    priv = UFO_CUT_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }

    G_OBJECT_CLASS (ufo_cut_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_cut_task_setup;
    iface->get_num_inputs = ufo_cut_task_get_num_inputs;
    iface->get_num_dimensions = ufo_cut_task_get_num_dimensions;
    iface->get_mode = ufo_cut_task_get_mode;
    iface->get_requisition = ufo_cut_task_get_requisition;
    iface->process = ufo_cut_task_process;
}

static void
ufo_cut_task_class_init (UfoCutTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_cut_task_set_property;
    oclass->get_property = ufo_cut_task_get_property;
    oclass->finalize = ufo_cut_task_finalize;

    properties[PROP_WIDTH] =
        g_param_spec_uint ("width",
            "Width of part to cut out",
            "Width of part to cut out",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoCutTaskPrivate));
}

static void
ufo_cut_task_init(UfoCutTask *self)
{
    self->priv = UFO_CUT_TASK_GET_PRIVATE(self);
    self->priv->width = 0;
}
