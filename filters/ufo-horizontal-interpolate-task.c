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

#include "ufo-horizontal-interpolate-task.h"


struct _UfoHorizontalInterpolateTaskPrivate {
    cl_kernel kernel;
    gboolean use_onesided_gradient;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoHorizontalInterpolateTask, ufo_horizontal_interpolate_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_HORIZONTAL_INTERPOLATE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_HORIZONTAL_INTERPOLATE_TASK, UfoHorizontalInterpolateTaskPrivate))

enum {
    PROP_0,
    PROP_USE_ONESIDED_GRADIENT,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_horizontal_interpolate_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_HORIZONTAL_INTERPOLATE_TASK, NULL));
}

static void
ufo_horizontal_interpolate_task_setup (UfoTask *task,
                       UfoResources *resources,
                       GError **error)
{
    UfoHorizontalInterpolateTaskPrivate *priv = UFO_HORIZONTAL_INTERPOLATE_TASK_GET_PRIVATE (task);

    priv->kernel = ufo_resources_get_kernel (resources, "interpolator.cl", "interpolate_mask_horizontally", NULL, error);

    if (priv->kernel != NULL) {
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
    }
}

static void
ufo_horizontal_interpolate_task_get_requisition (UfoTask *task,
                                 UfoBuffer **inputs,
                                 UfoRequisition *requisition,
                                 GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_horizontal_interpolate_task_get_num_inputs (UfoTask *task)
{
    return 2;
}

static guint
ufo_horizontal_interpolate_task_get_num_dimensions (UfoTask *task,
                                                    guint input)
{
    return 2;
}

static UfoTaskMode
ufo_horizontal_interpolate_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_horizontal_interpolate_task_equal_real (UfoNode *n1,
                                            UfoNode *n2)
{
    g_return_val_if_fail (UFO_IS_HORIZONTAL_INTERPOLATE_TASK (n1) && UFO_IS_HORIZONTAL_INTERPOLATE_TASK (n2), FALSE);
    return UFO_HORIZONTAL_INTERPOLATE_TASK (n1)->priv->kernel == UFO_HORIZONTAL_INTERPOLATE_TASK (n2)->priv->kernel;
}

static gboolean
ufo_horizontal_interpolate_task_process (UfoTask *task,
                                         UfoBuffer **inputs,
                                         UfoBuffer *output,
                                         UfoRequisition *requisition)
{
    UfoHorizontalInterpolateTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    cl_mem mask_in_mem, in_mem, out_mem;
    gint use_onesided_gradient;

    priv = UFO_HORIZONTAL_INTERPOLATE_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
    mask_in_mem = ufo_buffer_get_device_array (inputs[1], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    use_onesided_gradient = (gint) priv->use_onesided_gradient;

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &mask_in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 3, sizeof (cl_int), &use_onesided_gradient));
    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, requisition->dims, NULL);

    return TRUE;
}

static void
ufo_horizontal_interpolate_task_set_property (GObject *object,
                                              guint property_id,
                                              const GValue *value,
                                              GParamSpec *pspec)
{
    UfoHorizontalInterpolateTaskPrivate *priv = UFO_HORIZONTAL_INTERPOLATE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_USE_ONESIDED_GRADIENT:
            priv->use_onesided_gradient = g_value_get_boolean (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_horizontal_interpolate_task_get_property (GObject *object,
                                              guint property_id,
                                              GValue *value,
                                              GParamSpec *pspec)
{
    UfoHorizontalInterpolateTaskPrivate *priv = UFO_HORIZONTAL_INTERPOLATE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_USE_ONESIDED_GRADIENT:
            g_value_set_boolean (value, priv->use_onesided_gradient);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}
static void
ufo_horizontal_interpolate_task_finalize (GObject *object)
{
    UfoHorizontalInterpolateTaskPrivate *priv = UFO_HORIZONTAL_INTERPOLATE_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }

    G_OBJECT_CLASS (ufo_horizontal_interpolate_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_horizontal_interpolate_task_setup;
    iface->get_num_inputs = ufo_horizontal_interpolate_task_get_num_inputs;
    iface->get_num_dimensions = ufo_horizontal_interpolate_task_get_num_dimensions;
    iface->get_mode = ufo_horizontal_interpolate_task_get_mode;
    iface->get_requisition = ufo_horizontal_interpolate_task_get_requisition;
    iface->process = ufo_horizontal_interpolate_task_process;
}

static void
ufo_horizontal_interpolate_task_class_init (UfoHorizontalInterpolateTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);
    UfoNodeClass *node_class = UFO_NODE_CLASS (klass);

    oclass->set_property = ufo_horizontal_interpolate_task_set_property;
    oclass->get_property = ufo_horizontal_interpolate_task_get_property;
    oclass->finalize = ufo_horizontal_interpolate_task_finalize;
    node_class->equal = ufo_horizontal_interpolate_task_equal_real;

    properties[PROP_USE_ONESIDED_GRADIENT] =
        g_param_spec_boolean ("use-one-sided-gradient",
            "In case the mask spans all the way to the border, use two points on one side and fill with gradient",
            "In case the mask spans all the way to the border, use two points on one side and fill with gradient",
            TRUE, G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoHorizontalInterpolateTaskPrivate));
}

static void
ufo_horizontal_interpolate_task_init(UfoHorizontalInterpolateTask *self)
{
    self->priv = UFO_HORIZONTAL_INTERPOLATE_TASK_GET_PRIVATE(self);
    self->priv->use_onesided_gradient = TRUE;
}
