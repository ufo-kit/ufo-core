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

#include "ufo-pad-task.h"
#include "common/ufo-addressing.h"

struct _UfoPadTaskPrivate {
    /* OpenCL */
    cl_context context;
    cl_kernel kernel;
    cl_sampler sampler;

    /* properties */
    guint width, height;
    gint x, y;
    AddressingMode addressing_mode;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoPadTask, ufo_pad_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_PAD_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_PAD_TASK, UfoPadTaskPrivate))

enum {
    PROP_0,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_X,
    PROP_Y,
    PROP_ADDRESSING_MODE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void
change_sampler (UfoPadTaskPrivate *priv)
{
    cl_int err;

    if (priv->sampler) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseSampler (priv->sampler));
    }

    priv->sampler = clCreateSampler (priv->context,
                                     (cl_bool) TRUE,
                                     priv->addressing_mode,
                                     CL_FILTER_NEAREST,
                                     &err);

    UFO_RESOURCES_CHECK_CLERR (err);
}

UfoNode *
ufo_pad_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_PAD_TASK, NULL));
}

static void
ufo_pad_task_setup (UfoTask *task,
                    UfoResources *resources,
                    GError **error)
{
    UfoPadTaskPrivate *priv;

    priv = UFO_PAD_TASK_GET_PRIVATE (task);
    priv->context = ufo_resources_get_context (resources);
    priv->kernel = ufo_resources_get_kernel (resources, "pad.cl", "pad", NULL, error);
    change_sampler (priv);

    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);

    if (priv->kernel)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
}

static void
ufo_pad_task_get_requisition (UfoTask *task,
                              UfoBuffer **inputs,
                              UfoRequisition *requisition,
                              GError **error)
{
    UfoPadTaskPrivate *priv;
    UfoRequisition in_req;

    priv = UFO_PAD_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], &in_req);

    /* if width and height are not set make them as large as input */
    if (!priv->width) {
        priv->width = (guint) in_req.dims[0];
    }
    if (!priv->height) {
        priv->height = (guint) in_req.dims[1];
    }

    requisition->n_dims = 2;
    requisition->dims[0] = priv->width;
    requisition->dims[1] = priv->height;
}

static guint
ufo_pad_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_pad_task_get_num_dimensions (UfoTask *task,
                                 guint input)
{
    g_return_val_if_fail (input == 0, 0);

    return 2;
}

static UfoTaskMode
ufo_pad_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_pad_task_equal_real (UfoNode *n1,
                         UfoNode *n2)
{
    g_return_val_if_fail (UFO_IS_PAD_TASK (n2) && UFO_IS_PAD_TASK (n2), FALSE);

    return TRUE;
}

static gboolean
ufo_pad_task_process (UfoTask *task,
                      UfoBuffer **inputs,
                      UfoBuffer *output,
                      UfoRequisition *requisition)
{
    UfoPadTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    UfoRequisition in_req;
    cl_command_queue cmd_queue;
    cl_mem in_image, out_mem;
    cl_addressing_mode current_addressing_mode;
    gint input_shape[2];
    gint offset[2];

    priv = UFO_PAD_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], &in_req);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);

    /* change sampler mode if necessary */
    UFO_RESOURCES_CHECK_CLERR (clGetSamplerInfo (priv->sampler,
                                                 CL_SAMPLER_ADDRESSING_MODE,
                                                 sizeof (cl_addressing_mode),
                                                 &current_addressing_mode,
                                                 NULL));

    if (priv->addressing_mode != current_addressing_mode) {
        change_sampler (priv);
    }

    input_shape[0] = in_req.dims[0] - 1;
    input_shape[1] = in_req.dims[1] - 1;

    offset[0] = priv->x;
    offset[1] = priv->y;

    in_image = ufo_buffer_get_device_image (inputs[0], cmd_queue);

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &in_image));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_sampler), &priv->sampler));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 3, sizeof (cl_int2), input_shape));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 4, sizeof (cl_int2), offset));

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, requisition->dims, NULL);

    return TRUE;
}

static void
ufo_pad_task_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
    UfoPadTaskPrivate *priv = UFO_PAD_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_WIDTH:
            priv->width = g_value_get_uint (value);
            break;
        case PROP_HEIGHT:
            priv->height = g_value_get_uint (value);
            break;
        case PROP_X:
            priv->x = g_value_get_int (value);
            break;
        case PROP_Y:
            priv->y = g_value_get_int (value);
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
ufo_pad_task_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
    UfoPadTaskPrivate *priv = UFO_PAD_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_WIDTH:
            g_value_set_uint (value, priv->width);
            break;
        case PROP_HEIGHT:
            g_value_set_uint (value, priv->height);
            break;
        case PROP_X:
            g_value_set_int (value, priv->x);
            break;
        case PROP_Y:
            g_value_set_int (value, priv->y);
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
ufo_pad_task_finalize (GObject *object)
{
    UfoPadTaskPrivate *priv;

    priv = UFO_PAD_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }
    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }
    if (priv->sampler) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseSampler (priv->sampler));
        priv->sampler = NULL;
    }

    G_OBJECT_CLASS (ufo_pad_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_pad_task_setup;
    iface->get_num_inputs = ufo_pad_task_get_num_inputs;
    iface->get_num_dimensions = ufo_pad_task_get_num_dimensions;
    iface->get_mode = ufo_pad_task_get_mode;
    iface->get_requisition = ufo_pad_task_get_requisition;
    iface->process = ufo_pad_task_process;
}

static void
ufo_pad_task_class_init (UfoPadTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    UfoNodeClass *node_class = UFO_NODE_CLASS (klass);

    gobject_class->set_property = ufo_pad_task_set_property;
    gobject_class->get_property = ufo_pad_task_get_property;
    gobject_class->finalize = ufo_pad_task_finalize;

    properties[PROP_WIDTH] =
        g_param_spec_uint ("width",
            "Padded width",
            "Padded width",
            0, +32768, 0,
            G_PARAM_READWRITE);

    properties[PROP_HEIGHT] =
        g_param_spec_uint ("height",
            "Padded height",
            "Padded height",
            0, +32768, 0,
            G_PARAM_READWRITE);

    properties[PROP_X] =
        g_param_spec_int ("x",
            "X start index",
            "X index for input's 0th column",
            -32768, +32768, 0,
            G_PARAM_READWRITE);

    properties[PROP_Y] =
        g_param_spec_int ("y",
            "Y start index",
            "Y index for input's 0th row",
            -32768, +32768, 0,
            G_PARAM_READWRITE);

    properties[PROP_ADDRESSING_MODE] =
        g_param_spec_enum ("addressing-mode",
            "Outlier treatment (\"none\", \"clamp\", \"clamp_to_edge\", \"repeat\", \"mirrored_repeat\")",
            "Outlier treatment (\"none\", \"clamp\", \"clamp_to_edge\", \"repeat\", \"mirrored_repeat\")",
            g_enum_register_static ("ufo_pad_addressing_mode", addressing_values),
            CL_ADDRESS_CLAMP,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    node_class->equal = ufo_pad_task_equal_real;

    g_type_class_add_private (gobject_class, sizeof(UfoPadTaskPrivate));
}

static void
ufo_pad_task_init(UfoPadTask *self)
{
    self->priv = UFO_PAD_TASK_GET_PRIVATE(self);

    self->priv->width = 0;
    self->priv->height = 0;
    self->priv->x = 0;
    self->priv->y = 0;
    self->priv->addressing_mode = CL_ADDRESS_CLAMP;
}
