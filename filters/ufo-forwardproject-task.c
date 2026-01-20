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

#include "ufo-forwardproject-task.h"


struct _UfoForwardprojectTaskPrivate {
    cl_kernel kernel;
    cl_mem slice_mem;
    gfloat axis_pos;
    gfloat angle_step;
    guint num_projections;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoForwardprojectTask, ufo_forwardproject_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_FORWARDPROJECT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FORWARDPROJECT_TASK, UfoForwardprojectTaskPrivate))

enum {
    PROP_0,
    PROP_AXIS_POSITION,
    PROP_ANGLE_STEP,
    PROP_NUM_PROJECTIONS,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_forwardproject_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_FORWARDPROJECT_TASK, NULL));
}

static void
ufo_forwardproject_task_setup (UfoTask *task,
                               UfoResources *resources,
                               GError **error)
{
    UfoForwardprojectTaskPrivate *priv;

    priv = UFO_FORWARDPROJECT_TASK (task)->priv;

    priv->kernel = ufo_resources_get_kernel (resources, "forwardproject.cl", "forwardproject", NULL, error);

    if (priv->kernel != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);

    if (priv->angle_step == 0) 
        priv->angle_step = G_PI / priv->num_projections;
}

static void
ufo_forwardproject_task_get_requisition (UfoTask *task,
                                         UfoBuffer **inputs,
                                         UfoRequisition *requisition,
                                         GError **error)
{
    UfoForwardprojectTaskPrivate *priv;
    UfoRequisition in_req;

    priv = UFO_FORWARDPROJECT_TASK (task)->priv;

    ufo_buffer_get_requisition (inputs[0], &in_req);

    requisition->n_dims = 2;
    requisition->dims[0] = in_req.dims[0];
    requisition->dims[1] = priv->num_projections;
    if (priv->axis_pos == -G_MAXFLOAT) {
        priv->axis_pos = in_req.dims[0] / 2.0f;
    }
}

static guint
ufo_forwardproject_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_forwardproject_task_get_num_dimensions (UfoTask *task,
                               guint input)
{
    g_return_val_if_fail (input == 0, 0);
    return 2;
}

static UfoTaskMode
ufo_forwardproject_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_forwardproject_task_process (UfoTask *task,
                                 UfoBuffer **inputs,
                                 UfoBuffer *output,
                                 UfoRequisition *requisition)
{
    UfoForwardprojectTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    cl_mem in_mem;
    cl_mem out_mem;

    priv = UFO_FORWARDPROJECT_TASK (task)->priv;
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    in_mem = ufo_buffer_get_device_image (inputs[0], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (gfloat), &priv->axis_pos));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 3, sizeof (gfloat), &priv->angle_step));

    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, requisition->dims, NULL);

    return TRUE;
}

static void
ufo_forwardproject_task_set_property (GObject *object,
                                      guint property_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
    UfoForwardprojectTaskPrivate *priv = UFO_FORWARDPROJECT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_AXIS_POSITION:
            priv->axis_pos = g_value_get_float (value);
            break;
        case PROP_ANGLE_STEP:
            priv->angle_step = g_value_get_float(value);
            break;
        case PROP_NUM_PROJECTIONS:
            priv->num_projections = g_value_get_uint(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_forwardproject_task_get_property (GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
    UfoForwardprojectTaskPrivate *priv = UFO_FORWARDPROJECT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_AXIS_POSITION:
            g_value_set_float (value, priv->axis_pos);
            break;
        case PROP_ANGLE_STEP:
            g_value_set_float(value, priv->angle_step);
            break;
        case PROP_NUM_PROJECTIONS:
            g_value_set_uint(value, priv->num_projections);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_forwardproject_task_finalize (GObject *object)
{
    UfoForwardprojectTaskPrivate *priv;

    priv = UFO_FORWARDPROJECT_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }

    G_OBJECT_CLASS (ufo_forwardproject_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_forwardproject_task_setup;
    iface->get_requisition = ufo_forwardproject_task_get_requisition;
    iface->get_num_inputs = ufo_forwardproject_task_get_num_inputs;
    iface->get_num_dimensions = ufo_forwardproject_task_get_num_dimensions;
    iface->get_mode = ufo_forwardproject_task_get_mode;
    iface->process = ufo_forwardproject_task_process;
}

static void
ufo_forwardproject_task_class_init (UfoForwardprojectTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_forwardproject_task_set_property;
    gobject_class->get_property = ufo_forwardproject_task_get_property;
    gobject_class->finalize = ufo_forwardproject_task_finalize;

    properties[PROP_AXIS_POSITION] =
        g_param_spec_float ("axis-pos",
            "Position of rotation axis",
            "Position of rotation axis",
            -G_MAXFLOAT, G_MAXFLOAT, -G_MAXFLOAT,
            G_PARAM_READWRITE);

    properties[PROP_ANGLE_STEP] =
        g_param_spec_float("angle-step",
            "Increment of angle in radians",
            "Increment of angle in radians",
            -4.0f * ((gfloat) G_PI),
            +4.0f * ((gfloat) G_PI),
            0.0f,
            G_PARAM_READWRITE);

    properties[PROP_NUM_PROJECTIONS] =
        g_param_spec_uint("number",
            "Number of projections",
            "Number of projections",
            1, 32768, 256,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoForwardprojectTaskPrivate));
}

static void
ufo_forwardproject_task_init(UfoForwardprojectTask *self)
{
    self->priv = UFO_FORWARDPROJECT_TASK_GET_PRIVATE(self);

    self->priv->axis_pos = -G_MAXFLOAT;
    self->priv->num_projections = 256;
    self->priv->angle_step = 0;
}
