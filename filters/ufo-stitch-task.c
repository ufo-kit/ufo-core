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
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "ufo-stitch-task.h"

/* Number of input pixels processed by one work item in the parallel sum */
#define GLOBAL_SUM_HEIGHT 128


struct _UfoStitchTaskPrivate {
    gboolean adjust_mean;
    gboolean blend;
    gint shift;
    gint overlap;
    cl_context context;
    cl_kernel kernel, sum_kernel, pad_kernel;
    cl_mem sum_mem;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoStitchTask, ufo_stitch_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_STITCH_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_STITCH_TASK, UfoStitchTaskPrivate))

enum {
    PROP_0,
    PROP_SHIFT,
    PROP_ADJUST_MEAN,
    PROP_BLEND,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_stitch_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_STITCH_TASK, NULL));
}

static void
ufo_stitch_task_setup (UfoTask *task,
                       UfoResources *resources,
                       GError **error)
{
    UfoStitchTaskPrivate *priv;

    priv = UFO_STITCH_TASK_GET_PRIVATE (task);
    priv->context = ufo_resources_get_context (resources);
    priv->kernel = ufo_resources_get_kernel (resources, "interpolator.cl", "interpolate_horizontally", NULL, error);
    priv->sum_kernel = ufo_resources_get_kernel (resources, "reductor.cl", "parallel_sum_2D", NULL, error);
    priv->pad_kernel = ufo_resources_get_kernel (resources, "pad.cl", "pad_with_image", NULL, error);

    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);

    if (priv->kernel != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);

    if (priv->sum_kernel != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->sum_kernel), error);

    if (priv->pad_kernel != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->pad_kernel), error);

    priv->sum_mem = NULL;
}

static void
ufo_stitch_task_get_requisition (UfoTask *task,
                                 UfoBuffer **inputs,
                                 UfoRequisition *requisition,
                                 GError **error)
{
    UfoStitchTaskPrivate *priv;
    UfoRequisition left_req, right_req;

    priv = UFO_STITCH_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], &left_req);
    ufo_buffer_get_requisition (inputs[1], &right_req);

    if (left_req.dims[1] != right_req.dims[1]) {
        g_set_error_literal (error, UFO_TASK_ERROR, UFO_TASK_ERROR_GET_REQUISITION,
                             "Both stitch inputs must have the same height");
        return;
    }

    priv->overlap = (priv->shift >= 0) ? left_req.dims[0] - priv->shift : right_req.dims[0] + priv->shift;
    requisition->n_dims = 2;
    requisition->dims[0] = left_req.dims[0] + right_req.dims[0] - (gsize) priv->overlap;
    requisition->dims[1] = left_req.dims[1];
}

static guint
ufo_stitch_task_get_num_inputs (UfoTask *task)
{
    return 2;
}

static guint
ufo_stitch_task_get_num_dimensions (UfoTask *task,
                                    guint input)
{
    return 2;
}

static UfoTaskMode
ufo_stitch_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gfloat
compute_mean (UfoStitchTaskPrivate *priv,
              UfoProfiler *profiler,
              cl_command_queue cmd_queue,
              cl_mem input,
              gint width,
              gint height,
              gsize work_group_size,
              gint offset)
{
    gfloat mean = 0.0f, *summed_blocks;
    gsize global_work_size[2], num_blocks, i;
    gint groups_per_roi_width;
    cl_int cl_error;

    groups_per_roi_width = (priv->overlap + work_group_size - 1) / work_group_size;
    global_work_size[0] = groups_per_roi_width * work_group_size;
    global_work_size[1] = height / GLOBAL_SUM_HEIGHT;
    /* Number of output points (every work group produces 1 output value) */
    num_blocks = global_work_size[0] * global_work_size[1] / work_group_size,
    summed_blocks = (gfloat *) g_malloc (num_blocks * sizeof (gfloat));

    if (!priv->sum_mem) {
        priv->sum_mem = clCreateBuffer (priv->context,
                                        CL_MEM_WRITE_ONLY,
                                        num_blocks * sizeof (float),
                                        NULL,
                                        &cl_error);
        UFO_RESOURCES_CHECK_CLERR (cl_error);
    }

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->sum_kernel, 0, sizeof (cl_mem), &input));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->sum_kernel, 1, sizeof (cl_mem), &priv->sum_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->sum_kernel, 2, sizeof (float) * work_group_size, NULL));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->sum_kernel, 3, sizeof (gint), &offset));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->sum_kernel, 4, sizeof (gint), &width));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->sum_kernel, 5, sizeof (gint), &priv->overlap));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->sum_kernel, 6, sizeof (gint), &height));
    ufo_profiler_call (profiler, cmd_queue, priv->sum_kernel, 2, global_work_size, NULL);
    UFO_RESOURCES_CHECK_CLERR (clEnqueueReadBuffer (cmd_queue,
                                                    priv->sum_mem,
                                                    CL_TRUE,
                                                    0,
                                                    num_blocks * sizeof (float),
                                                    (void *) summed_blocks,
                                                    0, NULL, NULL));

    for (i = 0; i < num_blocks; i++)
        mean += summed_blocks[i];

    return mean / priv->overlap * height;
}

static gboolean
ufo_stitch_task_process (UfoTask *task,
                         UfoBuffer **inputs,
                         UfoBuffer *output,
                         UfoRequisition *requisition)
{
    UfoStitchTaskPrivate *priv;
    UfoProfiler *profiler;
    UfoGpuNode *node;
    UfoRequisition left_req, right_req;
    cl_command_queue cmd_queue;
    cl_mem left_mem;
    cl_mem right_mem;
    cl_mem out_mem;
    gint left, right, left_width, right_width, width, height, offset;
    gsize work_group_size;
    gsize global_work_size[2];
    size_t src_origin[3] = {0, 0, 0};
    size_t dst_origin[3] = {0, 0, 0};
    size_t region[3] = {0, 0, 1};
    size_t left_row_pitch;
    gfloat mean_left, mean_right, weight = 1.0f;

    priv = UFO_STITCH_TASK_GET_PRIVATE (task);
    /* If the shift is negative, it is the same as exchanging the left and right
     * image and changing the shift sign. */
    left = priv->shift >= 0 ? 0 : 1;
    right = priv->shift >= 0 ? 1 : 0;
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    work_group_size = g_value_get_ulong (ufo_gpu_node_get_info (node, UFO_GPU_NODE_INFO_MAX_WORK_GROUP_SIZE));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);

    left_mem = ufo_buffer_get_device_array (inputs[left], cmd_queue);
    right_mem = ufo_buffer_get_device_array (inputs[right], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);

    ufo_buffer_get_requisition (inputs[left], &left_req);
    ufo_buffer_get_requisition (inputs[right], &right_req);
    left_width = (gint) left_req.dims[0];
    right_width = (gint) right_req.dims[0];
    width = (gint) requisition->dims[0];
    height = (gint) requisition->dims[1];
    left_row_pitch = left_req.dims[0] * sizeof (float);
    offset = left_req.dims[0] - priv->overlap;

    if (priv->adjust_mean && priv->overlap) {
        /* Means of the overlapping region should match for both images to have
         * a nice transition, so compute the means and from that a weight which
         * will be used to adjust the right image in order to match the left
         * one. */
        mean_left = compute_mean (priv, profiler, cmd_queue, left_mem,
                                  left_width, height, work_group_size, offset);
        mean_right = compute_mean (priv, profiler, cmd_queue, right_mem,
                                   right_width, height, work_group_size, 0);
        weight = mean_left / mean_right;
    }

    /* Copy the left projection into the stitched one */
    region[0] = priv->blend ? (left_req.dims[0] - priv->overlap) * sizeof (float): left_row_pitch;
    region[1] = left_req.dims[1];
    UFO_RESOURCES_CHECK_CLERR (clEnqueueCopyBufferRect (cmd_queue,
                                                        left_mem, out_mem,
                                                        src_origin, dst_origin, region,
                                                        left_row_pitch, 0,
                                                        requisition->dims[0] * sizeof (float), 0,
                                                        0, NULL, NULL));

    if (priv->shift) {
        /* Copy the weighted right projection into the stitched one */
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->pad_kernel, 0, sizeof (cl_mem), &right_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->pad_kernel, 1, sizeof (cl_mem), &out_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->pad_kernel, 2, sizeof (gint), &priv->overlap));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->pad_kernel, 3, sizeof (gint), &left_width));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->pad_kernel, 4, sizeof (gint), &right_width));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->pad_kernel, 5, sizeof (gint), &width));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->pad_kernel, 6, sizeof (gfloat), &weight));
        global_work_size[0] = right_width - priv->overlap;
        global_work_size[1] = height;
        ufo_profiler_call (profiler, cmd_queue, priv->pad_kernel, 2, global_work_size, NULL);
    }

    if (priv->blend && priv->overlap) {
        /* Blend the overlapping region by linear interpolation */
        global_work_size[0] = priv->overlap;
        global_work_size[1] = height;

        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &left_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &right_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (cl_mem), &out_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 3, sizeof (gint), &width));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 4, sizeof (gint), &left_width));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 5, sizeof (gint), &right_width));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 6, sizeof (gint), &offset));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 7, sizeof (gfloat), &weight));
        ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, global_work_size, NULL);
    }

    return TRUE;
}

static void
ufo_stitch_task_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    UfoStitchTaskPrivate *priv = UFO_STITCH_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_SHIFT:
            priv->shift = g_value_get_int (value);
            break;
        case PROP_ADJUST_MEAN:
            priv->adjust_mean = g_value_get_boolean (value);
            break;
        case PROP_BLEND:
            priv->blend = g_value_get_boolean (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_stitch_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoStitchTaskPrivate *priv = UFO_STITCH_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_SHIFT:
            g_value_set_int (value, priv->shift);
            break;
        case PROP_ADJUST_MEAN:
            g_value_set_boolean (value, priv->adjust_mean);
            break;
        case PROP_BLEND:
            g_value_set_boolean (value, priv->blend);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_stitch_task_finalize (GObject *object)
{
    UfoStitchTaskPrivate *priv;

    priv = UFO_STITCH_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }

    if (priv->sum_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->sum_kernel));
        priv->sum_kernel = NULL;
    }

    if (priv->pad_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->pad_kernel));
        priv->pad_kernel = NULL;
    }

    if (priv->sum_mem) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->sum_mem));
        priv->sum_mem = NULL;
    }

    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }

    G_OBJECT_CLASS (ufo_stitch_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_stitch_task_setup;
    iface->get_num_inputs = ufo_stitch_task_get_num_inputs;
    iface->get_num_dimensions = ufo_stitch_task_get_num_dimensions;
    iface->get_mode = ufo_stitch_task_get_mode;
    iface->get_requisition = ufo_stitch_task_get_requisition;
    iface->process = ufo_stitch_task_process;
}

static void
ufo_stitch_task_class_init (UfoStitchTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_stitch_task_set_property;
    oclass->get_property = ufo_stitch_task_get_property;
    oclass->finalize = ufo_stitch_task_finalize;

    properties[PROP_SHIFT] =
        g_param_spec_int("shift",
            "If the second image is horizontally shifted by this value, the images will overlap (partially)",
            "If the second image is horizontally shifted by this value, the images will overlap (partially)",
            G_MININT, G_MAXINT, 0,
            G_PARAM_READWRITE);

    properties[PROP_ADJUST_MEAN] =
        g_param_spec_boolean ("adjust-mean",
            "Adjust second image's mean value based on the overlapping region",
            "Adjust second image's mean value based on the overlapping region",
            FALSE,
            G_PARAM_READWRITE);

    properties[PROP_BLEND] =
        g_param_spec_boolean ("blend",
            "Linearly interpolate between the first and the second image in the overlapping region",
            "Linearly interpolate between the first and the second image in the overlapping region",
            FALSE,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoStitchTaskPrivate));
}

static void
ufo_stitch_task_init(UfoStitchTask *self)
{
    self->priv = UFO_STITCH_TASK_GET_PRIVATE(self);
    self->priv->shift = 0;
    self->priv->adjust_mean = TRUE;
    self->priv->blend = FALSE;
}
