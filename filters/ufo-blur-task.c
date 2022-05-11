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
#include "ufo-blur-task.h"


struct _UfoBlurTaskPrivate {
    guint       size;
    gfloat      sigma;
    cl_context  context;
    cl_kernel   h_kernel;
    cl_kernel   v_kernel;
    cl_mem      weights_mem;
    cl_mem      intermediate_mem;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoBlurTask, ufo_blur_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_BLUR_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BLUR_TASK, UfoBlurTaskPrivate))

enum {
    PROP_0,
    PROP_SIZE,
    PROP_SIGMA,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_blur_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_BLUR_TASK, NULL));
}

static void
ufo_blur_task_setup (UfoTask *task,
                              UfoResources *resources,
                              GError **error)
{
    UfoBlurTaskPrivate *priv;

    priv = UFO_BLUR_TASK_GET_PRIVATE (task);

    priv->h_kernel = ufo_resources_get_kernel (resources, "gaussian.cl", "h_gaussian", NULL, error);

    if (error && *error)
        return;

    priv->v_kernel = ufo_resources_get_kernel (resources, "gaussian.cl", "v_gaussian", NULL, error);

    if (error && *error)
        return;

    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->h_kernel), error);
    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->v_kernel), error);

    priv->context = ufo_resources_get_context (resources);
    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);
}

static void
ufo_blur_task_get_requisition (UfoTask *task,
                               UfoBuffer **inputs,
                               UfoRequisition *requisition,
                               GError **error)
{
    UfoBlurTaskPrivate *priv;

    priv = UFO_BLUR_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], requisition);

    if (priv->weights_mem == NULL) {
        guint kernel_size;
        guint kernel_size_2;
        gfloat *weights;
        gfloat sum;
        cl_int err;

        kernel_size = priv->size;
        kernel_size_2 = kernel_size / 2;
        sum = 0.0;
        weights = g_malloc0 (kernel_size * sizeof(gfloat));

        for (guint i = 0; i < kernel_size_2 + 1; i++) {
            gfloat x = (gfloat) (kernel_size_2 - i);
            weights[i] = (gfloat) (1.0 / (priv->sigma * sqrt(2*G_PI)) * exp((x * x) / (-2.0 * priv->sigma * priv->sigma)));
            weights[kernel_size-i-1] = weights[i];
        }

        for (guint i = 0; i < kernel_size; i++)
            sum += weights[i];

        for (guint i = 0; i < kernel_size; i++)
            weights[i] /= sum;

        priv->weights_mem = clCreateBuffer (priv->context,
                                            CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                            kernel_size * sizeof(gfloat), weights, &err);
        UFO_RESOURCES_CHECK_CLERR (err);

        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->h_kernel, 2, sizeof(cl_mem), &priv->weights_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->v_kernel, 2, sizeof(cl_mem), &priv->weights_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->h_kernel, 3, sizeof(guint), &kernel_size_2));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->v_kernel, 3, sizeof(guint), &kernel_size_2));

        g_free(weights);
    }

    if (priv->intermediate_mem == NULL) {
        gsize size;
        cl_int err;
        
        size = requisition->dims[0] * requisition->dims[1] * sizeof (gfloat);
        priv->intermediate_mem = clCreateBuffer (priv->context,
                                                 CL_MEM_READ_WRITE,
                                                 size, NULL, &err);
        UFO_RESOURCES_CHECK_CLERR (err);
    }
}

static guint
ufo_blur_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_blur_task_get_num_dimensions (UfoTask *task,
                               guint input)
{
    g_return_val_if_fail (input == 0, 0);
    return 2;
}

static UfoTaskMode
ufo_blur_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_blur_task_process (UfoTask *task,
                                UfoBuffer **inputs,
                                UfoBuffer *output,
                                UfoRequisition *requisition)
{
    UfoBlurTaskPrivate *priv;
    UfoGpuNode *node;
    cl_command_queue cmd_queue;
    cl_mem in_mem;
    cl_mem out_mem;

    priv = UFO_BLUR_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);

    in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->h_kernel, 0, sizeof(cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->h_kernel, 1, sizeof(cl_mem), &priv->intermediate_mem));

    UFO_RESOURCES_CHECK_CLERR (clEnqueueNDRangeKernel (cmd_queue,
                                                       priv->h_kernel,
                                                       2, NULL, requisition->dims, NULL,
                                                       0, NULL, NULL));

    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->v_kernel, 0, sizeof(cl_mem), &priv->intermediate_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->v_kernel, 1, sizeof(cl_mem), &out_mem));

    UFO_RESOURCES_CHECK_CLERR (clEnqueueNDRangeKernel (cmd_queue,
                                                       priv->v_kernel,
                                                       2, NULL, requisition->dims, NULL,
                                                       0, NULL, NULL));
    return TRUE;
}

static void
ufo_blur_task_set_property (GObject *object,
                                     guint property_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
    UfoBlurTaskPrivate *priv = UFO_BLUR_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_SIZE:
            priv->size = g_value_get_uint(value);
            break;
        case PROP_SIGMA:
            priv->sigma = g_value_get_float(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_blur_task_get_property (GObject *object,
                                     guint property_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
    UfoBlurTaskPrivate *priv = UFO_BLUR_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_SIZE:
            g_value_set_uint(value, priv->size);
            break;
        case PROP_SIGMA:
            g_value_set_float(value, priv->sigma);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_blur_task_finalize (GObject *object)
{
    UfoBlurTaskPrivate *priv;

    priv = UFO_BLUR_TASK_GET_PRIVATE (object);

    if (priv->h_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->h_kernel));
        priv->h_kernel = NULL;
    }

    if (priv->v_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->v_kernel));
        priv->v_kernel = NULL;
    }

    if (priv->weights_mem) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->weights_mem));
        priv->weights_mem = NULL;
    }

    if (priv->intermediate_mem) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->intermediate_mem));
        priv->intermediate_mem = NULL;
    }

    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }

    G_OBJECT_CLASS (ufo_blur_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_blur_task_setup;
    iface->get_num_inputs = ufo_blur_task_get_num_inputs;
    iface->get_num_dimensions = ufo_blur_task_get_num_dimensions;
    iface->get_mode = ufo_blur_task_get_mode;
    iface->get_requisition = ufo_blur_task_get_requisition;
    iface->process = ufo_blur_task_process;
}

static void
ufo_blur_task_class_init (UfoBlurTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_blur_task_set_property;
    gobject_class->get_property = ufo_blur_task_get_property;
    gobject_class->finalize = ufo_blur_task_finalize;

    properties[PROP_SIZE] =
        g_param_spec_uint("size",
            "Size of the kernel",
            "Size of the kernel",
            3, 1000, 5,
            G_PARAM_READWRITE);

    properties[PROP_SIGMA] =
        g_param_spec_float("sigma",
            "sigma",
            "sigma",
            1.0f, 1000.0f, 1.0f,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoBlurTaskPrivate));
}

static void
ufo_blur_task_init(UfoBlurTask *self)
{
    self->priv = UFO_BLUR_TASK_GET_PRIVATE(self);

    self->priv->size = 5;
    self->priv->sigma = 1.0f;
    self->priv->weights_mem = NULL;
    self->priv->intermediate_mem = NULL;
}
