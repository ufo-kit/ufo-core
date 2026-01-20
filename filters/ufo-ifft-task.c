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

#include "ufo-ifft-task.h"
#include "common/ufo-fft.h"


struct _UfoIfftTaskPrivate {
    UfoFft *fft;
    UfoFftParameter param;

    cl_context context;
    cl_kernel kernel;

    gint crop_width;
    gint crop_height;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoIfftTask, ufo_ifft_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_IFFT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_IFFT_TASK, UfoIfftTaskPrivate))

enum {
    PROP_0,
    PROP_DIMENSIONS,
    PROP_CROP_WIDTH,
    PROP_CROP_HEIGHT,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_ifft_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_IFFT_TASK, NULL));
}

static void
ufo_ifft_task_setup (UfoTask *task,
                     UfoResources *resources,
                     GError **error)
{
    UfoIfftTaskPrivate *priv;

    priv = UFO_IFFT_TASK_GET_PRIVATE (task);
    priv->kernel = ufo_resources_get_kernel (resources, "fft.cl", "fft_pack", NULL, error);
    priv->context = ufo_resources_get_context (resources);

    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);

    if (priv->kernel != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
}

static void
ufo_ifft_task_get_requisition (UfoTask *task,
                               UfoBuffer **inputs,
                               UfoRequisition *requisition,
                               GError **error)
{
    UfoIfftTaskPrivate *priv;
    UfoRequisition in_req;
    cl_command_queue queue;

    if (ufo_buffer_get_layout (inputs[0]) != UFO_BUFFER_LAYOUT_COMPLEX_INTERLEAVED) {
        g_set_error_literal (error, UFO_TASK_ERROR, UFO_TASK_ERROR_GET_REQUISITION,
                             "ifft input must be complex");
        return;
    }

    priv = UFO_IFFT_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], &in_req);

    priv->param.zeropad = FALSE;
    priv->param.size[0] = in_req.dims[0] / 2;

    switch (priv->param.dimensions) {
        case UFO_FFT_1D:
            priv->param.batch = in_req.n_dims == 2 ? in_req.dims[1] : 1;
            break;

        case UFO_FFT_2D:
            priv->param.size[1] = in_req.dims[1];
            priv->param.batch = in_req.n_dims == 3 ? in_req.dims[2] : 1;
            break;

        case UFO_FFT_3D:
            break;
    }

    queue = ufo_gpu_node_get_cmd_queue (UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task))));
    UFO_RESOURCES_CHECK_SET_AND_RETURN (ufo_fft_update (priv->fft, priv->context, queue, &priv->param), error);

    *requisition = in_req;  /* keep third dimension for 2-D batching */
    requisition->dims[0] = priv->crop_width > 0 ? (gsize) priv->crop_width : priv->param.size[0];
    requisition->dims[1] = priv->crop_height > 0 ? (gsize) priv->crop_height : in_req.dims[1];
}

static guint
ufo_ifft_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_ifft_task_get_num_dimensions (UfoTask *task,
                                  guint input)
{
    g_return_val_if_fail (input == 0, 0);
    return UFO_IFFT_TASK_GET_PRIVATE (task)->param.dimensions > 2 ? 3 : 2;
}

static UfoTaskMode
ufo_ifft_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_ifft_task_equal_real (UfoNode *n1,
                          UfoNode *n2)
{
    g_return_val_if_fail (UFO_IS_IFFT_TASK (n1) && UFO_IS_IFFT_TASK (n2), FALSE);
    return TRUE;
}

static gboolean
ufo_ifft_task_process (UfoTask *task,
                       UfoBuffer **inputs,
                       UfoBuffer *output,
                       UfoRequisition *requisition)
{
    UfoIfftTaskPrivate *priv;
    UfoProfiler *profiler;
    UfoRequisition in_req;
    cl_mem in_mem;
    cl_mem out_mem;
    cl_int width;
    cl_int height;
    cl_command_queue queue;
    gfloat scale;
    gsize global_work_size[3];

    priv = UFO_IFFT_TASK_GET_PRIVATE (task);
    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    queue = ufo_gpu_node_get_cmd_queue (UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task))));
    in_mem = ufo_buffer_get_device_array (inputs[0], queue);
    out_mem = ufo_buffer_get_device_array (output, queue);

    if (ufo_buffer_get_layout (inputs[0]) != UFO_BUFFER_LAYOUT_COMPLEX_INTERLEAVED)
        g_warning ("ifft: input is not complex");

    /* In-place IFFT */
    UFO_RESOURCES_CHECK_CLERR (ufo_fft_execute (priv->fft, queue, profiler, in_mem, in_mem, UFO_FFT_BACKWARD,
                                                0, NULL, NULL));

    /* Scale and reshape if necessary */
    scale = 1.0f / ((gfloat) priv->param.size[0]);

    if (priv->param.dimensions == UFO_FFT_2D) {
        scale /= (gfloat) priv->param.size[1];
    }

    width = (cl_int) requisition->dims[0];
    height = (cl_int) requisition->dims[1];

    ufo_buffer_get_requisition (inputs[0], &in_req);
    ufo_buffer_set_layout (output, UFO_BUFFER_LAYOUT_REAL);

    global_work_size[0] = in_req.dims[0] >> 1;
    global_work_size[1] = in_req.dims[1];
    global_work_size[2] = requisition->n_dims == 3 ? in_req.dims[2] : 1;

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), (gpointer) &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), (gpointer) &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (cl_int), &width));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 3, sizeof (cl_int), &height));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 4, sizeof (gfloat), &scale));

    UFO_RESOURCES_CHECK_CLERR (clEnqueueNDRangeKernel (queue, priv->kernel,
                                                       3, NULL, global_work_size, NULL,
                                                       0, NULL, NULL));
    return TRUE;
}

static void
ufo_ifft_task_finalize (GObject *object)
{
    UfoIfftTaskPrivate *priv;

    priv = UFO_IFFT_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }

    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }

    if (priv->fft) {
        ufo_fft_destroy (priv->fft);
        priv->fft = NULL;
    }

    G_OBJECT_CLASS (ufo_ifft_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_ifft_task_setup;
    iface->get_requisition = ufo_ifft_task_get_requisition;
    iface->get_num_inputs = ufo_ifft_task_get_num_inputs;
    iface->get_num_dimensions = ufo_ifft_task_get_num_dimensions;
    iface->get_mode = ufo_ifft_task_get_mode;
    iface->process = ufo_ifft_task_process;
}

static void
ufo_ifft_task_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    UfoIfftTaskPrivate *priv = UFO_IFFT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_DIMENSIONS:
            priv->param.dimensions = g_value_get_uint (value);
            break;
        case PROP_CROP_WIDTH:
            priv->crop_width = g_value_get_int (value);
            break;
        case PROP_CROP_HEIGHT:
            priv->crop_height = g_value_get_int (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_ifft_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoIfftTaskPrivate *priv = UFO_IFFT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_DIMENSIONS:
            g_value_set_uint (value, priv->param.dimensions);
            break;
        case PROP_CROP_WIDTH:
            g_value_set_int (value, priv->crop_width);
            break;
        case PROP_CROP_HEIGHT:
            g_value_set_int (value, priv->crop_height);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_ifft_task_class_init (UfoIfftTaskClass *klass)
{
    GObjectClass *oclass;
    UfoNodeClass *node_class;

    oclass = G_OBJECT_CLASS (klass);
    node_class = UFO_NODE_CLASS (klass);

    oclass->finalize = ufo_ifft_task_finalize;
    oclass->set_property = ufo_ifft_task_set_property;
    oclass->get_property = ufo_ifft_task_get_property;

    properties[PROP_DIMENSIONS] =
        g_param_spec_uint ("dimensions",
            "Number of IFFT dimensions from 1 to 3",
            "Number of IFFT dimensions from 1 to 3",
            1, 3, 1,
            G_PARAM_READWRITE);

    properties[PROP_CROP_WIDTH] =
        g_param_spec_int ("crop-width",
            "Width of cropped output",
            "Width of cropped output",
            -1, G_MAXINT, -1,
            G_PARAM_READWRITE);

    properties[PROP_CROP_HEIGHT] =
        g_param_spec_int ("crop-height",
            "Height of cropped output",
            "Height of cropped output",
            -1, G_MAXINT, -1,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    node_class->equal = ufo_ifft_task_equal_real;

    g_type_class_add_private(klass, sizeof(UfoIfftTaskPrivate));
}

static void
ufo_ifft_task_init (UfoIfftTask *self)
{
    UfoIfftTaskPrivate *priv;
    self->priv = priv = UFO_IFFT_TASK_GET_PRIVATE (self);
    priv->crop_width = -1;
    priv->crop_height = -1;
    priv->kernel = NULL;
    priv->context = NULL;
    priv->fft = ufo_fft_new ();
    priv->param.dimensions = UFO_FFT_1D;
    priv->param.size[0] = 1;
    priv->param.size[1] = 1;
    priv->param.size[2] = 1;
    priv->param.batch = 1;
    priv->param.zeropad = FALSE;
}
