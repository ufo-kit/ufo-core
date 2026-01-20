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

#include <math.h>
#include "ufo-polar-coordinates-task.h"



typedef enum {
    DIRECTION_POLAR_TO_CARTESIAN,
    DIRECTION_CARTESIAN_TO_POLAR
} Direction;

static GEnumValue direction_values[] = {
    { DIRECTION_POLAR_TO_CARTESIAN, "DIRECTION_POLAR_TO_CARTESIAN", "polar_to_cartesian" },
    { DIRECTION_CARTESIAN_TO_POLAR, "DIRECTION_CARTESIAN_TO_POLAR", "cartesian_to_polar" },
    { 0, NULL, NULL }
};

struct _UfoPolarCoordinatesTaskPrivate {
    /* OpenCL */
    cl_context context;
    cl_kernel populate_polar_kernel;
    cl_kernel populate_cartesian_kernel;
    cl_sampler sampler;

    /* properties */
    guint width, height;
    Direction direction;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoPolarCoordinatesTask, ufo_polar_coordinates_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_POLAR_COORDINATES_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_POLAR_COORDINATES_TASK, UfoPolarCoordinatesTaskPrivate))

enum {
    PROP_0,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_DIRECTION,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_polar_coordinates_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_POLAR_COORDINATES_TASK, NULL));
}

static void
ufo_polar_coordinates_task_setup (UfoTask *task,
                                  UfoResources *resources,
                                  GError **error)
{
    UfoPolarCoordinatesTaskPrivate *priv;
    cl_int err;

    priv = UFO_POLAR_COORDINATES_TASK_GET_PRIVATE (task);
    priv->context = ufo_resources_get_context (resources);
    priv->populate_polar_kernel = ufo_resources_get_kernel (resources, "polar.cl", "populate_polar_space", NULL, error);
    priv->populate_cartesian_kernel = ufo_resources_get_kernel (resources, "polar.cl", "populate_cartesian_space", NULL, error);

    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);

    if (priv->populate_polar_kernel)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->populate_polar_kernel), error);

    if (priv->populate_cartesian_kernel)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->populate_cartesian_kernel), error);

    priv->sampler = clCreateSampler (priv->context,
                                     (cl_bool) FALSE,
                                     CL_ADDRESS_CLAMP_TO_EDGE,
                                     CL_FILTER_LINEAR,
                                     &err);

    UFO_RESOURCES_CHECK_SET_AND_RETURN (err, error);
}

static void
ufo_polar_coordinates_task_get_requisition (UfoTask *task,
                                            UfoBuffer **inputs,
                                            UfoRequisition *requisition,
                                            GError **error)
{
    UfoPolarCoordinatesTaskPrivate *priv;
    UfoRequisition in_req;
    guint n, width, height;
    gfloat angle_step;

    priv = UFO_POLAR_COORDINATES_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], &in_req);
    width = (guint) in_req.dims[0];
    height = (guint) in_req.dims[1];
    n = width > height ? width : height;
    angle_step = tan (2.0 / n);

    requisition->n_dims = 2;

    if (priv->width) {
        requisition->dims[0] = priv->width;
    }
    else {
        if (priv->direction == DIRECTION_CARTESIAN_TO_POLAR) {
            /* X-coordinate is the distance from [0, 0] which needs to cover every reachable
             * pixel, that pixel is located on the diagonal from the image center to one
             * of the corners. */
            requisition->dims[0] = (guint) round (sqrt (width * width / 4.0 + height * height / 4.0));
        }
        else {
            /* The polar coordinates width is the diagonal, if the user hasn't set
             * the final width assume the input before converting to polar
             * coordinates was square. */
            requisition->dims[0] = (guint) round (2 * in_req.dims[0] / sqrt (2));
        }
    }

    if (priv->height) {
        requisition->dims[1] = priv->height;
    }
    else {
        if (priv->direction == DIRECTION_CARTESIAN_TO_POLAR) {
            /* Y-coordinate is the angular position which needs to be sampled in such a
             * way that there is one pixel distance between two adjacent angular
             * positions and the distance from the image center to the maximum of (width
             * / 2, height / 2), where *width* and *half* is the original image shape.
             * */
            requisition->dims[1] = (guint) round (2 * G_PI / angle_step);
        }
        else {
            /* The polar coordinates width is the diagonal, if the user hasn't set
             * the final width assume the input before converting to polar
             * coordinates was square. */
            requisition->dims[1] = (guint) round (2 * in_req.dims[0] / sqrt (2));
        }
    }
}

static guint
ufo_polar_coordinates_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_polar_coordinates_task_get_num_dimensions (UfoTask *task,
                                               guint input)
{
    g_return_val_if_fail (input == 0, 0);

    return 2;
}

static UfoTaskMode
ufo_polar_coordinates_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_polar_coordinates_task_process (UfoTask *task,
                                    UfoBuffer **inputs,
                                    UfoBuffer *output,
                                    UfoRequisition *requisition)
{
    UfoPolarCoordinatesTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    UfoRequisition in_req;
    cl_command_queue cmd_queue;
    cl_mem image, out_mem;
    cl_kernel kernel;

    priv = UFO_POLAR_COORDINATES_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], &in_req);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    image = ufo_buffer_get_device_image (inputs[0], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    kernel = priv->direction == DIRECTION_CARTESIAN_TO_POLAR ? priv->populate_polar_kernel : priv->populate_cartesian_kernel;

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 0, sizeof (cl_mem), &image));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 1, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 2, sizeof (cl_sampler), &priv->sampler));

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, kernel, 2, requisition->dims, NULL);

    return TRUE;
}

static void
ufo_polar_coordinates_task_set_property (GObject *object,
                                         guint property_id,
                                         const GValue *value,
                                         GParamSpec *pspec)
{
    UfoPolarCoordinatesTaskPrivate *priv;

    priv = UFO_POLAR_COORDINATES_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_WIDTH:
            priv->width = g_value_get_uint (value);
            break;
        case PROP_HEIGHT:
            priv->height = g_value_get_uint (value);
            break;
        case PROP_DIRECTION:
            priv->direction = g_value_get_enum (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_polar_coordinates_task_get_property (GObject *object,
                                         guint property_id,
                                         GValue *value,
                                         GParamSpec *pspec)
{
    UfoPolarCoordinatesTaskPrivate *priv;

    priv = UFO_POLAR_COORDINATES_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_WIDTH:
            g_value_set_uint (value, priv->width);
            break;
        case PROP_HEIGHT:
            g_value_set_uint (value, priv->height);
            break;
        case PROP_DIRECTION:
            g_value_set_enum (value, priv->direction);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_polar_coordinates_task_finalize (GObject *object)
{
    UfoPolarCoordinatesTaskPrivate *priv;

    priv = UFO_POLAR_COORDINATES_TASK_GET_PRIVATE (object);

    if (priv->populate_polar_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->populate_polar_kernel));
        priv->populate_polar_kernel = NULL;
    }
    if (priv->populate_cartesian_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->populate_cartesian_kernel));
        priv->populate_cartesian_kernel = NULL;
    }
    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }
    if (priv->sampler) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseSampler (priv->sampler));
        priv->sampler = NULL;
    }

    G_OBJECT_CLASS (ufo_polar_coordinates_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_polar_coordinates_task_setup;
    iface->get_num_inputs = ufo_polar_coordinates_task_get_num_inputs;
    iface->get_num_dimensions = ufo_polar_coordinates_task_get_num_dimensions;
    iface->get_mode = ufo_polar_coordinates_task_get_mode;
    iface->get_requisition = ufo_polar_coordinates_task_get_requisition;
    iface->process = ufo_polar_coordinates_task_process;
}

static void
ufo_polar_coordinates_task_class_init (UfoPolarCoordinatesTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_polar_coordinates_task_set_property;
    oclass->get_property = ufo_polar_coordinates_task_get_property;
    oclass->finalize = ufo_polar_coordinates_task_finalize;

    properties[PROP_WIDTH] =
        g_param_spec_uint ("width",
            "Final width",
            "Final width after transformation",
            0, 32768, 0,
            G_PARAM_READWRITE);

    properties[PROP_HEIGHT] =
        g_param_spec_uint ("height",
            "Final height",
            "Final height after transformation",
            0, 32768, 0,
            G_PARAM_READWRITE);

    properties[PROP_DIRECTION] =
        g_param_spec_enum ("direction",
            "Conversion direction",
            "Conversion direction from: \"polar_to_cartesian\", \"cartesian_to_polar\"",
            g_enum_register_static ("ufo_polar_direction", direction_values),
            DIRECTION_CARTESIAN_TO_POLAR, G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoPolarCoordinatesTaskPrivate));
}

static void
ufo_polar_coordinates_task_init(UfoPolarCoordinatesTask *self)
{
    self->priv = UFO_POLAR_COORDINATES_TASK_GET_PRIVATE(self);

    self->priv->width = 0;
    self->priv->height = 0;
    self->priv->direction = DIRECTION_CARTESIAN_TO_POLAR;
}
