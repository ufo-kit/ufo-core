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

#include <glib.h>
#include <math.h>

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "common/ufo-addressing.h"
#include "common/ufo-interpolation.h"
#include "ufo-rotate-task.h"


struct _UfoRotateTaskPrivate {
    gfloat angle;
    gboolean reshape;
    gfloat center[2];
    gfloat padded_center[2];
    AddressingMode addressing_mode;
    Interpolation interpolation;

    cl_context context;
    cl_kernel kernel;
    cl_sampler sampler;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoRotateTask, ufo_rotate_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_ROTATE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_ROTATE_TASK, UfoRotateTaskPrivate))

enum {
    PROP_0,
    PROP_ANGLE,
    PROP_RESHAPE,
    PROP_CENTER,
    PROP_INTERPOLATION,
    PROP_ADDRESSING_MODE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static gint
ceil_anysign (gfloat number)
{
    return number < 0 ? ((gint) floor (number)) : ((gint) ceil (number));
}

/**
 * Compute how much do the extrema (corners) of the image shift with respect to
 * the global coordinates. Result is xmin, xmax, ymin, ymax. The minima
 * determine the shift of the center in the padded output and the combination of
 * maxima and minima determine the shape of the output in case reshape is True.
 */
static void
compute_extrema (gfloat *sincos, gint width, gint height, gint x_center, gint y_center, gint *extrema)
{
    gint i;
    gfloat x, y, x_min = G_MAXFLOAT, x_max = -G_MAXFLOAT, y_min = G_MAXFLOAT, y_max = -G_MAXFLOAT;
    /* Image corners shifted by the center */
    gfloat x_0[] = {-x_center, -x_center, width - x_center, width - x_center};
    gfloat y_0[] = {-y_center, height - y_center, -y_center, height - y_center};

    for (i = 0; i < 4; i++) {
        /* Apply rotation to the extrema to find the new ones */
        x = sincos[1] * x_0[i] - sincos[0] * y_0[i] + x_center;
        y = sincos[0] * x_0[i] + sincos[1] * y_0[i] + y_center;
        if (x < x_min) {
            x_min = x;
        }
        if (y < y_min) {
            y_min = y;
        }
        if (x > x_max) {
            x_max = x;
        }
        if (y > y_max) {
            y_max = y;
        }
    }

    /* Round up towards the edges of the image and make sure the original
     * coordinates stay in the result (all (x, y) from the original are also in
     * the reshaped image. */
    extrema[0] = MIN (ceil_anysign (x_min), 0);
    extrema[1] = MAX (ceil_anysign (x_max), width);
    extrema[2] = MIN (ceil_anysign (y_min), 0);
    extrema[3] = MAX (ceil_anysign (y_max), height);
}

UfoNode *
ufo_rotate_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_ROTATE_TASK, NULL));
}

static void
ufo_rotate_task_setup (UfoTask *task,
                       UfoResources *resources,
                       GError **error)
{
    UfoRotateTaskPrivate *priv;
    cl_int cl_error;

    priv = UFO_ROTATE_TASK_GET_PRIVATE (task);

    priv->context = ufo_resources_get_context (resources);
    priv->kernel = ufo_resources_get_kernel (resources, "rotate.cl", "rotate_image", NULL, error);
    /* Normalized coordinates are necessary for repeat addressing mode */
    priv->sampler = clCreateSampler (priv->context, (cl_bool) TRUE, priv->addressing_mode, priv->interpolation, &cl_error);

    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);
    UFO_RESOURCES_CHECK_SET_AND_RETURN (cl_error, error);

    if (priv->kernel)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
}

static void
ufo_rotate_task_get_requisition (UfoTask *task,
                                 UfoBuffer **inputs,
                                 UfoRequisition *requisition,
                                 GError **error)
{
    UfoRotateTaskPrivate *priv;
    UfoRequisition in_req;
    gint extrema[4];
    gfloat sincos[2];

    priv = UFO_ROTATE_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], &in_req);

    if (priv->center[0] == G_MAXFLOAT || priv->center[1] == G_MAXFLOAT) {
        priv->center[0] = in_req.dims[0] / 2.0f;
        priv->center[1] = in_req.dims[1] / 2.0f;
    }

    priv->padded_center[0] = priv->center[0];
    priv->padded_center[1] = priv->center[1];

    if (priv->reshape) {
        /* Make sure the complete original image stays in the field of view and
         * also that all the original (x, y) indices are involved as well. */
        sincos[0] = sin (priv->angle);
        sincos[1] = cos (priv->angle);
        compute_extrema (sincos, (gint) in_req.dims[0], (gint) in_req.dims[1],
                         priv->center[0], priv->center[1], extrema);
        requisition->n_dims = 2;
        requisition->dims[0] = extrema[1] - extrema[0];
        requisition->dims[1] = extrema[3] - extrema[2];
        priv->padded_center[0] -= extrema[0];
        priv->padded_center[1] -= extrema[2];
    }
    else {
        ufo_buffer_get_requisition (inputs[0], requisition);
    }
}

static guint
ufo_rotate_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_rotate_task_get_num_dimensions (UfoTask *task,
                                    guint input)
{
    return 2;
}

static UfoTaskMode
ufo_rotate_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_rotate_task_process (UfoTask *task,
                         UfoBuffer **inputs,
                         UfoBuffer *output,
                         UfoRequisition *requisition)
{
    UfoRotateTaskPrivate *priv;
    UfoProfiler *profiler;
    UfoGpuNode *node;
    UfoRequisition in_req;
    cl_command_queue cmd_queue;
    cl_mem in_mem;
    cl_mem out_mem;
    gint input_shape[2];
    gfloat sincos[2];

    priv = UFO_ROTATE_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    in_mem = ufo_buffer_get_device_image (inputs[0], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    ufo_buffer_get_requisition (inputs[0], &in_req);
    input_shape[0] = (gint) in_req.dims[0];
    input_shape[1] = (gint) in_req.dims[1];

    /* The kernel computes backward tranformation to avoid holes in the result.
     * This means that the original indices are computed from the rotated ones,
     * hence the angle sign change. */
    sincos[0] = sin (-priv->angle);
    sincos[1] = cos (-priv->angle);
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (cl_sampler), &priv->sampler));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 3, sizeof (cl_float2), sincos));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 4, sizeof (cl_float2), priv->center));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 5, sizeof (cl_float2), priv->padded_center));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 6, sizeof (cl_int2), input_shape));

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, requisition->dims, NULL);

    return TRUE;
}


static void
ufo_rotate_task_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    UfoRotateTaskPrivate *priv = UFO_ROTATE_TASK_GET_PRIVATE (object);
    GValueArray *array;

    switch (property_id) {
        case PROP_ANGLE:
            priv->angle = g_value_get_float (value);
            break;
        case PROP_RESHAPE:
            priv->reshape = g_value_get_boolean (value);
            break;
        case PROP_CENTER:
            array = (GValueArray *) g_value_get_boxed (value);
            priv->center[0] = g_value_get_float (g_value_array_get_nth (array, 0));
            priv->center[1] = g_value_get_float (g_value_array_get_nth (array, 1));
            break;
        case PROP_INTERPOLATION:
            priv->interpolation = g_value_get_enum (value);
            break;
        case PROP_ADDRESSING_MODE:
            priv->addressing_mode = g_value_get_enum (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_rotate_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoRotateTaskPrivate *priv = UFO_ROTATE_TASK_GET_PRIVATE (object);
    GValueArray *array;
    GValue x = G_VALUE_INIT;

    switch (property_id) {
        case PROP_ANGLE:
            g_value_set_float (value, priv->angle);
            break;
        case PROP_RESHAPE:
            g_value_set_boolean (value, priv->reshape);
            break;
        case PROP_CENTER:
            array = g_value_array_new (2);
            g_value_init (&x, G_TYPE_FLOAT);
            g_value_set_float (&x, priv->center[0]);
            g_value_array_append (array, &x);
            g_value_set_float (&x, priv->center[1]);
            g_value_array_append (array, &x);
            g_value_take_boxed (value, array);
            break;
        case PROP_INTERPOLATION:
            g_value_set_enum (value, priv->interpolation);
            break;
        case PROP_ADDRESSING_MODE:
            g_value_set_enum (value, priv->addressing_mode);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_rotate_task_finalize (GObject *object)
{
    UfoRotateTaskPrivate *priv;

    priv = UFO_ROTATE_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }

    if (priv->sampler) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseSampler (priv->sampler));
        priv->sampler = NULL;
    }

    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }

    G_OBJECT_CLASS (ufo_rotate_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_rotate_task_setup;
    iface->get_num_inputs = ufo_rotate_task_get_num_inputs;
    iface->get_num_dimensions = ufo_rotate_task_get_num_dimensions;
    iface->get_mode = ufo_rotate_task_get_mode;
    iface->get_requisition = ufo_rotate_task_get_requisition;
    iface->process = ufo_rotate_task_process;
}

static void
ufo_rotate_task_class_init (UfoRotateTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_rotate_task_set_property;
    oclass->get_property = ufo_rotate_task_get_property;
    oclass->finalize = ufo_rotate_task_finalize;

    GParamSpec *region_vals = g_param_spec_float ("float-region-values",
                                                  "Float Region values",
                                                  "Elements in float regions",
                                                  -G_MAXFLOAT,
                                                  G_MAXFLOAT,
                                                  0.0f,
                                                  G_PARAM_READWRITE);

    properties[PROP_ANGLE] =
        g_param_spec_float ("angle",
            "Rotation angle in radians",
            "Rotation angle in radians",
            -G_MAXFLOAT, G_MAXFLOAT, 0.0f,
            G_PARAM_READWRITE);

    properties[PROP_RESHAPE] =
        g_param_spec_boolean ("reshape",
            "Reshape the image to fit the rotated original",
            "Reshape the image to fit the rotated original",
            FALSE,
            G_PARAM_READWRITE);

    properties[PROP_CENTER] =
        g_param_spec_value_array ("center",
            "Center of rotation (x, y)",
            "Center of rotation (x, y)",
            region_vals,
            G_PARAM_READWRITE);

    properties[PROP_ADDRESSING_MODE] =
        g_param_spec_enum ("addressing-mode",
            "Outlier treatment (\"none\", \"clamp\", \"clamp_to_edge\", \"repeat\", \"mirrored_repeat\")",
            "Outlier treatment (\"none\", \"clamp\", \"clamp_to_edge\", \"repeat\", \"mirrored_repeat\")",
            g_enum_register_static ("ufo_rot_addressing_mode", addressing_values),
            CL_ADDRESS_CLAMP,
            G_PARAM_READWRITE);

    properties[PROP_INTERPOLATION] =
        g_param_spec_enum ("interpolation",
            "Interpolation (\"nearest\" or \"linear\")",
            "Interpolation (\"nearest\" or \"linear\")",
            g_enum_register_static ("ufo_rot_interpolation", interpolation_values),
            CL_FILTER_LINEAR,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoRotateTaskPrivate));
}

static void
ufo_rotate_task_init(UfoRotateTask *self)
{
    self->priv = UFO_ROTATE_TASK_GET_PRIVATE (self);
    self->priv->angle = 0;
    self->priv->reshape = FALSE;
    self->priv->addressing_mode = CL_ADDRESS_CLAMP;
    self->priv->interpolation = CL_FILTER_LINEAR;
    self->priv->center[0] = G_MAXFLOAT;
    self->priv->center[1] = G_MAXFLOAT;
}
