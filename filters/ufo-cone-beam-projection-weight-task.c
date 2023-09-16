/*
 * Copyright (C) 2011-2017 Karlsruhe Institute of Technology
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

#include <math.h>
#include <glib.h>
#include <glib-object.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "ufo-cone-beam-projection-weight-task.h"
#include "common/ufo-scarray.h"


struct _UfoConeBeamProjectionWeightTaskPrivate {
    /* Properties */
    UfoScarray *center_position_x, *center_position_z, *source_distance, *detector_distance, *axis_angle_x;
    /* Private */
    guint count;
    /* OpenCL */
    cl_context context;
    cl_kernel kernel;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoConeBeamProjectionWeightTask, ufo_cone_beam_projection_weight_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK, ufo_task_interface_init))

#define UFO_CONE_BEAM_PROJECTION_WEIGHT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CONE_BEAM_PROJECTION_WEIGHT_TASK, UfoConeBeamProjectionWeightTaskPrivate))

enum {
    PROP_0,
    PROP_CENTER_POSITION_X,
    PROP_CENTER_POSITION_Z,
    PROP_SOURCE_DISTANCE,
    PROP_DETECTOR_DISTANCE,
    PROP_AXIS_ANGLE_X,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_cone_beam_projection_weight_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_CONE_BEAM_PROJECTION_WEIGHT_TASK, NULL));
}

static void
ufo_cone_beam_projection_weight_task_setup (UfoTask *task,
                                            UfoResources *resources,
                                            GError **error)
{
    UfoConeBeamProjectionWeightTaskPrivate *priv;

    priv = UFO_CONE_BEAM_PROJECTION_WEIGHT_TASK_GET_PRIVATE (task);
    priv->context = ufo_resources_get_context (resources);
    priv->kernel = ufo_resources_get_kernel (resources, "conebeam.cl", "weight_projection", NULL, error);

    UFO_RESOURCES_CHECK_CLERR (clRetainContext (priv->context));
    if (priv->kernel != NULL) {
        UFO_RESOURCES_CHECK_CLERR (clRetainKernel (priv->kernel));
    }
}

static void
ufo_cone_beam_projection_weight_task_get_requisition (UfoTask *task,
                                                      UfoBuffer **inputs,
                                                      UfoRequisition *requisition,
                                                      GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_cone_beam_projection_weight_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_cone_beam_projection_weight_task_get_num_dimensions (UfoTask *task,
                                                         guint input)
{
    g_return_val_if_fail (input == 0, 0);

    return 2;
}

static UfoTaskMode
ufo_cone_beam_projection_weight_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_cone_beam_projection_weight_task_process (UfoTask *task,
                                              UfoBuffer **inputs,
                                              UfoBuffer *output,
                                              UfoRequisition *requisition)
{
    UfoConeBeamProjectionWeightTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    cl_mem in_mem;
    cl_mem out_mem;
    cl_float source_distance, detector_distance, overall_distance, magnification_recip, cos_angle, center[2];

    priv = UFO_CONE_BEAM_PROJECTION_WEIGHT_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    cos_angle = (cl_float) cos (ufo_scarray_get_double (priv->axis_angle_x, priv->count));
    center[0] = (cl_float) ufo_scarray_get_double (priv->center_position_x, priv->count);
    center[1] = (cl_float) ufo_scarray_get_double (priv->center_position_z, priv->count);
    source_distance = (cl_float) ufo_scarray_get_double (priv->source_distance, priv->count);
    detector_distance = (cl_float) ufo_scarray_get_double (priv->detector_distance, priv->count);
    overall_distance = source_distance + detector_distance;
    magnification_recip = source_distance / overall_distance;
    if (cos_angle > 0.9999999f) {
        overall_distance = source_distance;
    }

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof(cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof(cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof(cl_float2), &center));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 3, sizeof(cl_float), &source_distance));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 4, sizeof(cl_float), &overall_distance));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 5, sizeof(cl_float), &magnification_recip));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 6, sizeof(cl_float), &cos_angle));
    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, requisition->dims, NULL);
    priv->count++;

    return TRUE;
}


static void
ufo_cone_beam_projection_weight_task_set_property (GObject *object,
                                                   guint property_id,
                                                   const GValue *value,
                                                   GParamSpec *pspec)
{
    UfoConeBeamProjectionWeightTaskPrivate *priv = UFO_CONE_BEAM_PROJECTION_WEIGHT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_CENTER_POSITION_X:
            ufo_scarray_get_value (priv->center_position_x, value);
            break;
        case PROP_CENTER_POSITION_Z:
            ufo_scarray_get_value (priv->center_position_z, value);
            break;
        case PROP_SOURCE_DISTANCE:
            ufo_scarray_get_value (priv->source_distance, value);
            break;
        case PROP_DETECTOR_DISTANCE:
            ufo_scarray_get_value (priv->detector_distance, value);
            break;
        case PROP_AXIS_ANGLE_X:
            ufo_scarray_get_value (priv->axis_angle_x, value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_cone_beam_projection_weight_task_get_property (GObject *object,
                                                   guint property_id,
                                                   GValue *value,
                                                   GParamSpec *pspec)
{
    UfoConeBeamProjectionWeightTaskPrivate *priv = UFO_CONE_BEAM_PROJECTION_WEIGHT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_CENTER_POSITION_X:
            ufo_scarray_set_value (priv->center_position_x, value);
            break;
        case PROP_CENTER_POSITION_Z:
            ufo_scarray_set_value (priv->center_position_z, value);
            break;
        case PROP_SOURCE_DISTANCE:
            ufo_scarray_set_value (priv->source_distance, value);
            break;
        case PROP_DETECTOR_DISTANCE:
            ufo_scarray_set_value (priv->detector_distance, value);
            break;
        case PROP_AXIS_ANGLE_X:
            ufo_scarray_set_value (priv->axis_angle_x, value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_cone_beam_projection_weight_task_finalize (GObject *object)
{
    UfoConeBeamProjectionWeightTaskPrivate *priv;

    priv = UFO_CONE_BEAM_PROJECTION_WEIGHT_TASK_GET_PRIVATE (object);
    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }

    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }

    G_OBJECT_CLASS (ufo_cone_beam_projection_weight_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_cone_beam_projection_weight_task_setup;
    iface->get_num_inputs = ufo_cone_beam_projection_weight_task_get_num_inputs;
    iface->get_num_dimensions = ufo_cone_beam_projection_weight_task_get_num_dimensions;
    iface->get_mode = ufo_cone_beam_projection_weight_task_get_mode;
    iface->get_requisition = ufo_cone_beam_projection_weight_task_get_requisition;
    iface->process = ufo_cone_beam_projection_weight_task_process;
}

static void
ufo_cone_beam_projection_weight_task_class_init (UfoConeBeamProjectionWeightTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_cone_beam_projection_weight_task_set_property;
    oclass->get_property = ufo_cone_beam_projection_weight_task_get_property;
    oclass->finalize = ufo_cone_beam_projection_weight_task_finalize;

    GParamSpec *double_region_vals = g_param_spec_double ("double-region-values",
                                                          "Double Region values",
                                                          "Elements in double regions",
                                                          -INFINITY,
                                                          INFINITY,
                                                          0.0,
                                                          G_PARAM_READWRITE);

    properties[PROP_CENTER_POSITION_X] =
        g_param_spec_value_array ("center-position-x",
                                  "Global x center (horizontal in a projection) of the volume with respect to projections",
                                  "Global x center (horizontal in a projection) of the volume with respect to projections",
                                  double_region_vals,
                                  G_PARAM_READWRITE);

    properties[PROP_CENTER_POSITION_Z] =
        g_param_spec_value_array ("center-position-z",
                                  "Global z center (vertical in a projection) of the volume with respect to projections",
                                  "Global z center (vertical in a projection) of the volume with respect to projections",
                                  double_region_vals,
                                  G_PARAM_READWRITE);

    properties[PROP_SOURCE_DISTANCE] =
        g_param_spec_value_array ("source-distance",
                                  "Distance from source to the volume center",
                                  "Distance from source to the volume center",
                                  double_region_vals,
                                  G_PARAM_READWRITE);

    properties[PROP_DETECTOR_DISTANCE] =
        g_param_spec_value_array ("detector-distance",
                                  "Distance from detector to the volume center",
                                  "Distance from detector to the volume center",
                                  double_region_vals,
                                  G_PARAM_READWRITE);

    properties[PROP_AXIS_ANGLE_X] =
        g_param_spec_value_array("axis-angle-x",
                                 "Rotation axis rotation around the x-axis (laminographic angle [rad], 0 = tomography)",
                                 "Rotation axis rotation around the x-axis (laminographic angle [rad], 0 = tomography)",
                                 double_region_vals,
                                 G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoConeBeamProjectionWeightTaskPrivate));
}

static void
ufo_cone_beam_projection_weight_task_init(UfoConeBeamProjectionWeightTask *self)
{
    self->priv = UFO_CONE_BEAM_PROJECTION_WEIGHT_TASK_GET_PRIVATE(self);

    self->priv->center_position_x = ufo_scarray_new (0, G_TYPE_DOUBLE, NULL);
    self->priv->center_position_z = ufo_scarray_new (0, G_TYPE_DOUBLE, NULL);
    self->priv->source_distance = ufo_scarray_new (0, G_TYPE_DOUBLE, NULL);
    self->priv->detector_distance = ufo_scarray_new (0, G_TYPE_DOUBLE, NULL);
    self->priv->axis_angle_x = ufo_scarray_new (1, G_TYPE_DOUBLE, NULL);
    self->priv->count = 0;
}
