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

#include "ufo-rescale-task.h"


typedef enum {
    INTERPOLATION_NEAREST = CL_FILTER_NEAREST,
    INTERPOLATION_LINEAR = CL_FILTER_LINEAR,
} Interpolation;

static GEnumValue interpolation_values[] = {
    { INTERPOLATION_NEAREST,   "INTERPOLATION_NEAREST",   "nearest" },
    { INTERPOLATION_LINEAR,    "INTERPOLATION_LINEAR",    "linear" },
    { 0, NULL, NULL}
};

struct _UfoRescaleTaskPrivate {
    cl_context context;
    cl_kernel kernel;
    Interpolation interpolation;
    gfloat x_factor;
    gfloat y_factor;
    guint width;
    guint height;
    cl_sampler sampler;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoRescaleTask, ufo_rescale_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_RESCALE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_RESCALE_TASK, UfoRescaleTaskPrivate))

enum {
    PROP_0,
    PROP_FACTOR,
    PROP_X_FACTOR,
    PROP_Y_FACTOR,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_INTERPOLATION,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_rescale_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_RESCALE_TASK, NULL));
}

static void
ufo_rescale_task_setup (UfoTask *task,
                           UfoResources *resources,
                           GError **error)
{
    UfoRescaleTaskPrivate *priv;
    cl_int err;

    priv = UFO_RESCALE_TASK_GET_PRIVATE (task);
    priv->context = ufo_resources_get_context (resources);
    priv->kernel = ufo_resources_get_kernel (resources, "rescale.cl", "rescale", NULL, error);

    /* We can afford CL_ADDRESS_NONE if the final shape is rounded down */
    priv->sampler = clCreateSampler (priv->context,
                                     (cl_bool) FALSE,
                                     CL_ADDRESS_NONE,
                                     (cl_filter_mode) priv->interpolation,
                                     &err);

    UFO_RESOURCES_CHECK_SET_AND_RETURN (err, error);
    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);

    if (priv->kernel != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
}

static void
ufo_rescale_task_get_requisition (UfoTask *task,
                                  UfoBuffer **inputs,
                                  UfoRequisition *requisition,
                                  GError **error)
{
    UfoRescaleTaskPrivate *priv;
    UfoRequisition in_req;

    priv = UFO_RESCALE_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], &in_req);

    requisition->n_dims = 2;
    requisition->dims[0] = (gint) (priv->width > 0 ? priv->width : (in_req.dims[0] * priv->x_factor));
    requisition->dims[1] = (gint) (priv->height > 0 ? priv->height : (in_req.dims[1] * priv->y_factor));

    /* If the factors are too big we want at least one row/column in order */
    /* not to have a buffer with 0 in any dimension */
    if (requisition->dims[0] == 0) {
        requisition->dims[0] = 1;
    }

    if (requisition->dims[1] == 0) {
        requisition->dims[1] = 1;
    }
}

static guint
ufo_rescale_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_rescale_task_get_num_dimensions (UfoTask *task,
                               guint input)
{
    g_return_val_if_fail (input == 0, 0);
    return 2;
}

static UfoTaskMode
ufo_rescale_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_rescale_task_process (UfoTask *task,
                          UfoBuffer **inputs,
                          UfoBuffer *output,
                          UfoRequisition *requisition)
{
    UfoRescaleTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    UfoRequisition in_req;
    cl_command_queue *cmd_queue;
    cl_mem in_mem;
    cl_mem out_mem;
    gfloat x_factor;
    gfloat y_factor;

    priv = UFO_RESCALE_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE(task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    in_mem = ufo_buffer_get_device_image (inputs[0], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);

    ufo_buffer_get_requisition (inputs[0], &in_req);
    x_factor = priv->width > 0 ? ((gfloat) priv->width) / in_req.dims[0] : priv->x_factor;
    y_factor = priv->height > 0 ? ((gfloat) priv->height) / in_req.dims[1] : priv->y_factor;

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (cl_sampler), &priv->sampler));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 3, sizeof (gfloat), &x_factor));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 4, sizeof (gfloat), &y_factor));

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, requisition->dims, NULL);

    return TRUE;
}

static void
ufo_rescale_task_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
    UfoRescaleTaskPrivate *priv = UFO_RESCALE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_FACTOR:
            priv->x_factor = g_value_get_float (value);
            priv->y_factor = g_value_get_float (value);
            break;
        case PROP_X_FACTOR:
            priv->x_factor = g_value_get_float (value);
            break;
        case PROP_Y_FACTOR:
            priv->y_factor = g_value_get_float (value);
            break;
        case PROP_WIDTH:
            priv->width = g_value_get_uint (value);
            break;
        case PROP_HEIGHT:
            priv->height = g_value_get_uint (value);
            break;
        case PROP_INTERPOLATION:
            priv->interpolation = g_value_get_enum (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_rescale_task_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
    UfoRescaleTaskPrivate *priv = UFO_RESCALE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_FACTOR:
            if (priv->x_factor != priv->y_factor)
                g_warning ("rescale: no common factor");
            else
                g_value_set_float (value, priv->x_factor);
            break;
        case PROP_X_FACTOR:
            g_value_set_float (value, priv->x_factor);
            break;
        case PROP_Y_FACTOR:
            g_value_set_float (value, priv->y_factor);
            break;
        case PROP_WIDTH:
            g_value_set_uint (value, priv->width);
            break;
        case PROP_HEIGHT:
            g_value_set_uint (value, priv->height);
            break;
        case PROP_INTERPOLATION:
            g_value_set_enum (value, priv->interpolation);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_rescale_task_finalize (GObject *object)
{
    UfoRescaleTaskPrivate *priv;

    priv = UFO_RESCALE_TASK_GET_PRIVATE (object);

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

    G_OBJECT_CLASS (ufo_rescale_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_rescale_task_setup;
    iface->get_requisition = ufo_rescale_task_get_requisition;
    iface->get_num_inputs = ufo_rescale_task_get_num_inputs;
    iface->get_num_dimensions = ufo_rescale_task_get_num_dimensions;
    iface->get_mode = ufo_rescale_task_get_mode;
    iface->process = ufo_rescale_task_process;
}

static void
ufo_rescale_task_class_init (UfoRescaleTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_rescale_task_set_property;
    gobject_class->get_property = ufo_rescale_task_get_property;
    gobject_class->finalize = ufo_rescale_task_finalize;

    properties[PROP_FACTOR] =
        g_param_spec_float ("factor",
            "Rescale factor",
            "Rescale factor for both dimensions, e.g. 0.5 reduces width and height by half",
            1e-3, G_MAXFLOAT, 2.0f,
            G_PARAM_READWRITE);

    properties[PROP_X_FACTOR] =
        g_param_spec_float ("x-factor",
            "Rescale x-factor",
            "Rescale x-factor, e.g. 0.5 reduces width by half",
            1e-3, G_MAXFLOAT, 2.0f,
            G_PARAM_READWRITE);

    properties[PROP_Y_FACTOR] =
        g_param_spec_float ("y-factor",
            "Rescale y-factor",
            "Rescale y-factor, e.g. 0.5 reduces height by half",
            1e-3, G_MAXFLOAT, 2.0f,
            G_PARAM_READWRITE);

    properties[PROP_INTERPOLATION] =
        g_param_spec_enum ("interpolation",
            "Interpolation mode (\"nearest\", \"linear\")",
            "Interpolation mode (\"nearest\", \"linear\")",
            g_enum_register_static ("ufo_rescale_interpolation", interpolation_values),
            INTERPOLATION_LINEAR, G_PARAM_READWRITE);

    properties[PROP_WIDTH] =
        g_param_spec_uint ("width",
            "Ignore x-factor and use specified width",
            "Ignore x-factor and use specified width",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE);

    properties[PROP_HEIGHT] =
        g_param_spec_uint ("height",
            "Ignore y-factor and use specified height",
            "Ignore y-factor and use specified height",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoRescaleTaskPrivate));
}

static void
ufo_rescale_task_init(UfoRescaleTask *self)
{
    self->priv = UFO_RESCALE_TASK_GET_PRIVATE(self);
    self->priv->x_factor = 2.0f;
    self->priv->y_factor = 2.0f;
    self->priv->width = 0;
    self->priv->height = 0;
    self->priv->interpolation = INTERPOLATION_LINEAR;
}
