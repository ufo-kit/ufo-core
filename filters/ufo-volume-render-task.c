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
#include <math.h>

#include "ufo-volume-render-task.h"

/**
 * SECTION:ufo-volume-render-task
 * @Short_description: Project volume data onto 2D plane
 * @Title: volume_render
 */

struct _UfoVolumeRenderTaskPrivate {
    cl_context   context;
    cl_kernel    kernel;
    gfloat      *view_matrix;
    cl_mem       view_mem;
    cl_mem       volume_mem;
    guint        current;
    gfloat       angle;

    guint   width;
    guint   height;
    guint   n_generate;
    gfloat  step;
    gfloat  delta;
    gfloat  threshold;
    gfloat  slope;
    gfloat  constant;
    gfloat  displacement;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoVolumeRenderTask, ufo_volume_render_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_VOLUME_RENDER_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_VOLUME_RENDER_TASK, UfoVolumeRenderTaskPrivate))

enum {
    PROP_0,
    PROP_STEP,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_NUM_GENERATE,
    PROP_DELTA,
    PROP_THRESHOLD,
    PROP_SLOPE,
    PROP_CONSTANT,
    PROP_DISPLACEMENT,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_volume_render_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_VOLUME_RENDER_TASK, NULL));
}

static void
ufo_volume_render_task_setup (UfoTask *task,
                              UfoResources *resources,
                              GError **error)
{
    UfoVolumeRenderTaskPrivate *priv;
    cl_int err;

    priv = UFO_VOLUME_RENDER_TASK_GET_PRIVATE (task);

    priv->context = ufo_resources_get_context (resources);
    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);

    priv->kernel = ufo_resources_get_kernel (resources, "volume.cl", "rayCastVolume", NULL, error);
    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);

    priv->view_matrix = g_malloc0 (4 * 4 * sizeof(gfloat));
    priv->view_matrix[0] = 1.0f;
    priv->view_matrix[5] = 1.0f;
    priv->view_matrix[10] = 1.0f;
    priv->view_matrix[15] = 1.0f;

    priv->view_mem = clCreateBuffer (priv->context,
                                     CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                     4 * 4 * sizeof(gfloat), priv->view_matrix, &err);
    UFO_RESOURCES_CHECK_SET_AND_RETURN (err, error);
}

static void
ufo_volume_render_task_get_requisition (UfoTask *task,
                                        UfoBuffer **inputs,
                                        UfoRequisition *requisition,
                                        GError **error)
{
    UfoVolumeRenderTaskPrivate *priv;

    cl_image_format volume_format = {
        .image_channel_order = CL_LUMINANCE,
        .image_channel_data_type = CL_UNORM_INT8
    };

    priv = UFO_VOLUME_RENDER_TASK_GET_PRIVATE (task);

    if (priv->volume_mem == NULL) {
        UfoRequisition req;
        cl_int err;

        ufo_buffer_get_requisition (inputs[0], &req);
        g_assert (req.n_dims == 3);

        priv->volume_mem = clCreateImage3D (priv->context,
                                            CL_MEM_READ_ONLY,
                                            &volume_format,
                                            req.dims[0], req.dims[1], req.dims[2],
                                            0, 0, NULL, &err);
        UFO_RESOURCES_CHECK_CLERR (err);
    }

    requisition->n_dims = 2;
    requisition->dims[0] = (gsize) priv->width;
    requisition->dims[1] = (gsize) priv->height;
}

static guint
ufo_volume_render_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_volume_render_task_get_num_dimensions (UfoTask *task, guint input)
{
    g_return_val_if_fail (input == 0, 0);
    return 3;
}

static UfoTaskMode
ufo_volume_render_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_REDUCTOR | UFO_TASK_MODE_GPU;
}

static void
rotate (gfloat view_matrix[],
        gfloat angle)
{
    const gfloat cos_angle = (gfloat) cos(angle);
    const gfloat sin_angle = (gfloat) sin(angle);

    view_matrix[5] = cos_angle;
    view_matrix[6] = -sin_angle;
    view_matrix[9] = sin_angle;
    view_matrix[10] = cos_angle;
}

static gboolean
ufo_volume_render_task_process (UfoTask *task,
                                UfoBuffer **inputs,
                                UfoBuffer *output,
                                UfoRequisition *requisition)
{
    UfoVolumeRenderTaskPrivate *priv;
    UfoGpuNode *node;
    UfoRequisition req;
    cl_command_queue cmd_queue;
    cl_mem in_mem;
    size_t origin[] = { 0, 0, 0 };

    priv = UFO_VOLUME_RENDER_TASK_GET_PRIVATE (task);

    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
    ufo_buffer_get_requisition (inputs[0], &req);

    UFO_RESOURCES_CHECK_CLERR (clEnqueueCopyBufferToImage (cmd_queue,
                                                           in_mem, priv->volume_mem,
                                                           0, origin, req.dims,
                                                           0, NULL, NULL));

    return TRUE;
}

static gboolean
ufo_volume_render_task_generate (UfoTask *task,
                                 UfoBuffer *output,
                                 UfoRequisition *requisition)
{
    UfoVolumeRenderTaskPrivate *priv;
    UfoGpuNode *node;
    cl_command_queue cmd_queue;
    cl_mem render_mem;
    cl_uint steps;

    priv = UFO_VOLUME_RENDER_TASK_GET_PRIVATE (task);

    if (priv->current == priv->n_generate)
        return FALSE;

    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    render_mem = ufo_buffer_get_device_array (output, cmd_queue);
    steps = (cl_uint) ((1.414f + fabs (priv->displacement)) / priv->step);

    UFO_RESOURCES_CHECK_CLERR (clEnqueueWriteBuffer (cmd_queue,
                                                     priv->view_mem, CL_FALSE,
                                                     0, 4 * 4 * sizeof (gfloat), priv->view_matrix,
                                                     0, NULL, NULL));

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &priv->volume_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &render_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (cl_mem), &priv->view_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 3, sizeof (cl_uint), &steps));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 4, sizeof (gfloat), &priv->step));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 5, sizeof (gfloat), &priv->displacement));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 6, sizeof (gfloat), &priv->slope));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 7, sizeof (gfloat), &priv->constant));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 8, sizeof (gfloat), &priv->threshold));

    UFO_RESOURCES_CHECK_CLERR (clEnqueueNDRangeKernel (cmd_queue,
                                                       priv->kernel,
                                                       2, NULL, requisition->dims, NULL,
                                                       0, NULL, NULL));

    priv->current++;
    priv->angle += priv->delta;
    rotate (priv->view_matrix, priv->angle);

    return TRUE;
}

static void
ufo_volume_render_task_set_property (GObject *object,
                                     guint property_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
    UfoVolumeRenderTaskPrivate *priv = UFO_VOLUME_RENDER_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_WIDTH:
            priv->width = g_value_get_uint (value);
            break;
        case PROP_HEIGHT:
            priv->height = g_value_get_uint (value);
            break;
        case PROP_NUM_GENERATE:
            priv->n_generate = g_value_get_uint (value);
            break;
        case PROP_DELTA:
            priv->delta = g_value_get_float (value);
            break;
        case PROP_STEP:
            priv->step = g_value_get_float (value);
            break;
        case PROP_THRESHOLD:
            priv->threshold = g_value_get_float (value);
            break;
        case PROP_SLOPE:
            priv->slope = g_value_get_float (value);
            break;
        case PROP_CONSTANT:
            priv->constant = g_value_get_float (value);
            break;
        case PROP_DISPLACEMENT:
            priv->displacement = g_value_get_float (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_volume_render_task_get_property (GObject *object,
                                     guint property_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
    UfoVolumeRenderTaskPrivate *priv = UFO_VOLUME_RENDER_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_WIDTH:
            g_value_set_uint (value, priv->width);
            break;
        case PROP_HEIGHT:
            g_value_set_uint (value, priv->height);
            break;
        case PROP_NUM_GENERATE:
            g_value_set_uint (value, priv->n_generate);
            break;
        case PROP_DELTA:
            g_value_set_float (value, priv->delta);
            break;
        case PROP_STEP:
            g_value_set_float (value, priv->threshold);
            break;
        case PROP_THRESHOLD:
            g_value_set_float (value, priv->threshold);
            break;
        case PROP_SLOPE:
            g_value_set_float (value, priv->slope);
            break;
        case PROP_CONSTANT:
            g_value_set_float (value, priv->constant);
            break;
        case PROP_DISPLACEMENT:
            g_value_set_float (value, priv->displacement);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_volume_render_task_finalize (GObject *object)
{
    UfoVolumeRenderTaskPrivate *priv;

    priv = UFO_VOLUME_RENDER_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }

    if (priv->volume_mem) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->volume_mem));
        priv->volume_mem = NULL;
    }

    if (priv->view_mem) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->view_mem));
        priv->view_mem = NULL;
    }

    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }

    G_OBJECT_CLASS (ufo_volume_render_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_volume_render_task_setup;
    iface->get_num_inputs = ufo_volume_render_task_get_num_inputs;
    iface->get_num_dimensions = ufo_volume_render_task_get_num_dimensions;
    iface->get_mode = ufo_volume_render_task_get_mode;
    iface->get_requisition = ufo_volume_render_task_get_requisition;
    iface->process = ufo_volume_render_task_process;
    iface->generate = ufo_volume_render_task_generate;
}

static void
ufo_volume_render_task_class_init (UfoVolumeRenderTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_volume_render_task_set_property;
    gobject_class->get_property = ufo_volume_render_task_get_property;
    gobject_class->finalize = ufo_volume_render_task_finalize;

    properties[PROP_WIDTH] =
        g_param_spec_uint ("width",
            "Width",
            "Width of the rendered image",
            1, 32768, 512,
            G_PARAM_READWRITE);

    properties[PROP_HEIGHT] =
        g_param_spec_uint ("height",
            "Height",
            "Height of the rendered image",
            1, 32768, 512,
            G_PARAM_READWRITE);

    properties[PROP_NUM_GENERATE] = 
        g_param_spec_uint ("num-generate",
            "Number of rendered views",
            "Number of rendered views",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    properties[PROP_DELTA] =
        g_param_spec_float ("delta",
            "Delta between angles",
            "Delta between angles in radians",
            -G_MAXFLOAT, G_MAXFLOAT, 0.025f,
            G_PARAM_READWRITE);

    properties[PROP_STEP] =
        g_param_spec_float ("step",
            "Delta between angles",
            "Delta between angles in radians",
            G_MINFLOAT, G_MAXFLOAT, 0.025f,
            G_PARAM_READWRITE);

    properties[PROP_THRESHOLD] =
        g_param_spec_float ("threshold",
            "Threshold",
            "Threshold",
            -G_MAXFLOAT, G_MAXFLOAT, 0.0f,
            G_PARAM_READWRITE);

    properties[PROP_SLOPE] =
        g_param_spec_float ("slope",
            "Slope of the alpha function",
            "Slope of the alpha function",
            -G_MAXFLOAT, G_MAXFLOAT, 1.0f,
            G_PARAM_READWRITE);

    properties[PROP_CONSTANT] =
        g_param_spec_float ("constant",
            "Constant of the alpha function",
            "Constant of the alpha function",
            -G_MAXFLOAT, G_MAXFLOAT, 0.0f,
            G_PARAM_READWRITE);

    properties[PROP_DISPLACEMENT] =
        g_param_spec_float ("displacement",
            "Displacement of the near plane",
            "Displacement of the near plane",
            -G_MAXFLOAT, G_MAXFLOAT, 0.0f,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoVolumeRenderTaskPrivate));
}

static void
ufo_volume_render_task_init(UfoVolumeRenderTask *self)
{
    self->priv = UFO_VOLUME_RENDER_TASK_GET_PRIVATE(self);
    self->priv->width = 512;
    self->priv->height = 512;
    self->priv->n_generate = 1;
    self->priv->threshold = 0.0f;
    self->priv->slope = 1.0f;
    self->priv->constant = 0.0f;
    self->priv->displacement = 0.0f;
    self->priv->current = 0;
    self->priv->angle = 0.0f;
    self->priv->delta = 0.025f;
    self->priv->step = 0.025f;
}
