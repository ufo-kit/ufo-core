/*
 * Copyright (C) 2011-2019 Karlsruhe Institute of Technology
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

#include "ufo-find-large-spots-task.h"
#include "common/ufo-addressing.h"
#include "common/ufo-common.h"

typedef enum {
    SPOT_THRESHOLD_BELOW = -1,
    SPOT_THRESHOLD_ABSOLUTE,
    SPOT_THRESHOLD_ABOVE
} SpotThresholdMode;

static GEnumValue spot_threshold_mode_values[] = {
    { SPOT_THRESHOLD_BELOW, "SPOT_THRESHOLD_BELOW", "below" },
    { SPOT_THRESHOLD_ABSOLUTE, "SPOT_THRESHOLD_ABSOLUTE", "absolute" },
    { SPOT_THRESHOLD_ABOVE, "SPOT_THRESHOLD_ABOVE", "above" },
    { 0, NULL, NULL}
};

struct _UfoFindLargeSpotsTaskPrivate {
    gfloat spot_threshold;
    SpotThresholdMode spot_threshold_mode;
    gfloat grow_threshold;
    cl_context context;
    cl_kernel set_ones_kernel, set_threshold_kernel, grow_kernel, holes_kernel, convolution_kernel, sum_kernel;
    cl_sampler sampler;
    cl_mem aux_mem[2], counter_mem;
    AddressingMode addressing_mode;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoFindLargeSpotsTask, ufo_find_large_spots_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_FIND_LARGE_SPOTS_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FIND_LARGE_SPOTS_TASK, UfoFindLargeSpotsTaskPrivate))

enum {
    PROP_0,
    PROP_SPOT_THRESHOLD_MODE,
    PROP_SPOT_THRESHOLD,
    PROP_GROW_THRESHOLD,
    PROP_ADDRESSING_MODE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_find_large_spots_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_FIND_LARGE_SPOTS_TASK, NULL));
}

static void
ufo_find_large_spots_task_setup (UfoTask *task,
                                 UfoResources *resources,
                                 GError **error)
{
    cl_int err;
    UfoFindLargeSpotsTaskPrivate *priv = UFO_FIND_LARGE_SPOTS_TASK_GET_PRIVATE (task);

    priv->context = ufo_resources_get_context (resources);
    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);

    priv->set_ones_kernel = ufo_resources_get_kernel (resources, "morphology.cl", "set_to_ones", NULL, error);
    priv->set_threshold_kernel = ufo_resources_get_kernel (resources, "morphology.cl", "set_above_threshold", NULL, error);
    priv->grow_kernel = ufo_resources_get_kernel (resources, "morphology.cl", "grow_region_above_threshold", NULL, error);
    priv->holes_kernel = ufo_resources_get_kernel (resources, "morphology.cl", "fill_holes", NULL, error);
    priv->convolution_kernel = ufo_resources_get_kernel (resources, "estimate-noise.cl", "convolve_abs_laplacian_diff", NULL, error);
    priv->sum_kernel = ufo_resources_get_kernel (resources, "reductor.cl", "reduce_M_SUM", NULL, error);
    for (gint i = 0; i < 2; i++) {
        priv->aux_mem[i] = NULL;
    }
    priv->counter_mem = NULL;

    if (priv->set_ones_kernel) {
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->set_ones_kernel), error);
    }
    if (priv->set_threshold_kernel) {
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->set_threshold_kernel), error);
    }
    if (priv->grow_kernel) {
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->grow_kernel), error);
    }
    if (priv->holes_kernel) {
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->holes_kernel), error);
    }
    if (priv->convolution_kernel) {
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->convolution_kernel), error);
    }
    if (priv->sum_kernel) {
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->sum_kernel), error);
    }
    priv->sampler = clCreateSampler (priv->context,
                                     (cl_bool) TRUE,
                                     priv->addressing_mode,
                                     CL_FILTER_NEAREST,
                                     &err);
    UFO_RESOURCES_CHECK_CLERR (err);
}

static void
ufo_find_large_spots_task_get_requisition (UfoTask *task,
                                           UfoBuffer **inputs,
                                           UfoRequisition *requisition,
                                           GError **error)
{
    cl_int err;
    UfoFindLargeSpotsTaskPrivate *priv = UFO_FIND_LARGE_SPOTS_TASK_GET_PRIVATE (task);

    ufo_buffer_get_requisition (inputs[0], requisition);

    if (!priv->aux_mem[0]) {
        for (gint i = 0; i < 2; i++) {
            priv->aux_mem[i] = clCreateBuffer (priv->context,
                                               CL_MEM_READ_WRITE,
                                               sizeof (cl_float) * requisition->dims[0] * requisition->dims[1],
                                               NULL,
                                               &err);
            UFO_RESOURCES_CHECK_CLERR (err);
        }
        priv->counter_mem = clCreateBuffer (priv->context,
                                            CL_MEM_READ_WRITE,
                                            sizeof (cl_int),
                                            NULL,
                                            &err);
        UFO_RESOURCES_CHECK_CLERR (err);
    }
}

static guint
ufo_find_large_spots_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_find_large_spots_task_get_num_dimensions (UfoTask *task,
                                              guint input)
{
    return 2;
}

static UfoTaskMode
ufo_find_large_spots_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_find_large_spots_task_process (UfoTask *task,
                                   UfoBuffer **inputs,
                                   UfoBuffer *output,
                                   UfoRequisition *requisition)
{
    UfoFindLargeSpotsTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    GValue *max_work_group_size_gvalue;
    gsize max_work_group_size;
    cl_command_queue cmd_queue;
    cl_mem in_mem;
    cl_mem out_mem;
    gfloat estimated_sigma;
    gsize global_size[2];
    gint counter = 1, fill_pattern = 0;

    priv = UFO_FIND_LARGE_SPOTS_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    global_size[0] = requisition->dims[0] - 2;
    global_size[1] = requisition->dims[1] - 2;

    if (priv->grow_threshold <= 0.0f) {
        max_work_group_size_gvalue = ufo_gpu_node_get_info (node, UFO_GPU_NODE_INFO_MAX_WORK_GROUP_SIZE);
        max_work_group_size = g_value_get_ulong (max_work_group_size_gvalue);
        g_value_unset (max_work_group_size_gvalue);
        in_mem = ufo_buffer_get_device_image (inputs[0], cmd_queue);
        estimated_sigma = ufo_common_estimate_sigma (priv->convolution_kernel,
                                                     priv->sum_kernel,
                                                     cmd_queue,
                                                     priv->sampler,
                                                     profiler,
                                                     in_mem,
                                                     out_mem,
                                                     max_work_group_size,
                                                     requisition->dims);
        /* IF not specified, make grow_threshold FWTM of the assumed noise normal distribution. */
        priv->grow_threshold = 4.29 * estimated_sigma;
        g_debug ("Estimated sigma: %g", estimated_sigma);
    }
    in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);

    /* First set the mask to 1 where spot_threshold is exceeded */
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->set_threshold_kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->set_threshold_kernel, 1, sizeof (cl_mem), &priv->aux_mem[0]));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->set_threshold_kernel, 2, sizeof (cl_int), &priv->spot_threshold));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->set_threshold_kernel, 3, sizeof (cl_int), &priv->spot_threshold_mode));
    ufo_profiler_call (profiler, cmd_queue, priv->set_threshold_kernel, 2, requisition->dims, NULL);
    UFO_RESOURCES_CHECK_CLERR (clEnqueueCopyBuffer (cmd_queue,
                                                    priv->aux_mem[0],
                                                    priv->aux_mem[1],
                                                    0, 0, sizeof (cl_float) * requisition->dims[0] * requisition->dims[1],
                                                    0, NULL, NULL));

    while (counter) {
        /* Grow the seeds until the difference between surrouding pixels is less
         * than grow_threshold. */
        /* Reset counter */
        UFO_RESOURCES_CHECK_CLERR (clEnqueueFillBuffer (cmd_queue, priv->counter_mem, &fill_pattern, sizeof (cl_int),
                                                        0, sizeof (cl_int), 0, NULL, NULL));

        /* Grow region */
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->grow_kernel, 0, sizeof (cl_mem), &in_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->grow_kernel, 1, sizeof (cl_mem), &priv->aux_mem[0]));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->grow_kernel, 2, sizeof (cl_mem), &priv->aux_mem[1]));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->grow_kernel, 3, sizeof (cl_mem), &priv->counter_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->grow_kernel, 4, sizeof (cl_int), &priv->grow_threshold));
        ufo_profiler_call (profiler, cmd_queue, priv->grow_kernel, 2, global_size, NULL);
        /* New visited is the last grown. */
        UFO_RESOURCES_CHECK_CLERR (clEnqueueCopyBuffer (cmd_queue,
                                                        priv->aux_mem[1],
                                                        priv->aux_mem[0],
                                                        0, 0, sizeof (cl_float) * requisition->dims[0] * requisition->dims[1],
                                                        0, NULL, NULL));
        UFO_RESOURCES_CHECK_CLERR (clEnqueueReadBuffer (cmd_queue,
                                                        priv->counter_mem,
                                                        CL_TRUE,
                                                        0,
                                                        sizeof (cl_int),
                                                        &counter,
                                                        0, NULL, NULL));
    }

    /* Initialize resulting mask to ones except borders and pixels where current
     * mask is set. This will serve for growing the boundary towards the center,
     * i.e. filling holes. From now on, aux_mem[0] is the fixed mask with holes
     * and aux_mem[1] with out_mem are the result/visited pair.
     */
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->set_ones_kernel, 0, sizeof (cl_mem), &priv->aux_mem[1]));
    ufo_profiler_call (profiler, cmd_queue, priv->set_ones_kernel, 2, global_size, NULL);
    UFO_RESOURCES_CHECK_CLERR (clEnqueueCopyBuffer (cmd_queue,
                                                    priv->aux_mem[1],
                                                    out_mem,
                                                    0, 0, sizeof (cl_float) * requisition->dims[0] * requisition->dims[1],
                                                    0, NULL, NULL));

    counter = 1;
    while (counter) {
        /* Fill holes by progressing from the edges towards the center and
         * taking into account the pre-computed mask. */
        /* Reset counter */
        UFO_RESOURCES_CHECK_CLERR (clEnqueueFillBuffer (cmd_queue, priv->counter_mem, &fill_pattern, sizeof (cl_int),
                                                        0, sizeof (cl_int), 0, NULL, NULL));

        /* Grow region towards the center */
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->holes_kernel, 0, sizeof (cl_mem), &priv->aux_mem[0]));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->holes_kernel, 1, sizeof (cl_mem), &priv->aux_mem[1]));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->holes_kernel, 2, sizeof (cl_mem), &out_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->holes_kernel, 3, sizeof (cl_mem), &priv->counter_mem));
        ufo_profiler_call (profiler, cmd_queue, priv->holes_kernel, 2, global_size, NULL);
        /* New visited is the last grown. */
        UFO_RESOURCES_CHECK_CLERR (clEnqueueCopyBuffer (cmd_queue,
                                                        out_mem,
                                                        priv->aux_mem[1],
                                                        0, 0, sizeof (cl_float) * requisition->dims[0] * requisition->dims[1],
                                                        0, NULL, NULL));
        UFO_RESOURCES_CHECK_CLERR (clEnqueueReadBuffer (cmd_queue,
                                                        priv->counter_mem,
                                                        CL_TRUE,
                                                        0,
                                                        sizeof (cl_int),
                                                        &counter,
                                                        0, NULL, NULL));
    }

    return TRUE;
}


static void
ufo_find_large_spots_task_set_property (GObject *object,
                                        guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
    UfoFindLargeSpotsTaskPrivate *priv = UFO_FIND_LARGE_SPOTS_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_SPOT_THRESHOLD:
            priv->spot_threshold = g_value_get_float (value);
            break;
        case PROP_SPOT_THRESHOLD_MODE:
            priv->spot_threshold_mode = g_value_get_enum (value);
            break;
        case PROP_GROW_THRESHOLD:
            priv->grow_threshold = g_value_get_float (value);
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
ufo_find_large_spots_task_get_property (GObject *object,
                                        guint property_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
    UfoFindLargeSpotsTaskPrivate *priv = UFO_FIND_LARGE_SPOTS_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_SPOT_THRESHOLD:
            g_value_set_float (value, priv->spot_threshold);
            break;
        case PROP_SPOT_THRESHOLD_MODE:
            g_value_set_enum (value, priv->spot_threshold_mode);
            break;
        case PROP_GROW_THRESHOLD:
            g_value_set_float (value, priv->grow_threshold);
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
ufo_find_large_spots_task_finalize (GObject *object)
{
    UfoFindLargeSpotsTaskPrivate *priv = UFO_FIND_LARGE_SPOTS_TASK_GET_PRIVATE (object);

    if (priv->set_ones_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->set_ones_kernel));
        priv->set_ones_kernel = NULL;
    }
    if (priv->set_threshold_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->set_threshold_kernel));
        priv->set_threshold_kernel = NULL;
    }
    if (priv->grow_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->grow_kernel));
        priv->grow_kernel = NULL;
    }
    if (priv->holes_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->holes_kernel));
        priv->holes_kernel = NULL;
    }
    if (priv->convolution_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->convolution_kernel));
        priv->convolution_kernel = NULL;
    }
    if (priv->sum_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->sum_kernel));
        priv->sum_kernel = NULL;
    }
    if (priv->sampler) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseSampler (priv->sampler));
        priv->sampler = NULL;
    }
    for (gint i = 0; i < 2; i++) {
        if (priv->aux_mem[i]) {
            UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->aux_mem[i]));
            priv->aux_mem[i] = NULL;
        }
    }
    if (priv->counter_mem) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->counter_mem));
        priv->counter_mem = NULL;
    }
    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }

    G_OBJECT_CLASS (ufo_find_large_spots_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_find_large_spots_task_setup;
    iface->get_num_inputs = ufo_find_large_spots_task_get_num_inputs;
    iface->get_num_dimensions = ufo_find_large_spots_task_get_num_dimensions;
    iface->get_mode = ufo_find_large_spots_task_get_mode;
    iface->get_requisition = ufo_find_large_spots_task_get_requisition;
    iface->process = ufo_find_large_spots_task_process;
}

static void
ufo_find_large_spots_task_class_init (UfoFindLargeSpotsTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_find_large_spots_task_set_property;
    oclass->get_property = ufo_find_large_spots_task_get_property;
    oclass->finalize = ufo_find_large_spots_task_finalize;

    properties[PROP_SPOT_THRESHOLD] =
        g_param_spec_float ("spot-threshold",
            "Pixels with grey value larger than this are considered as spots",
            "Pixels with grey value larger than this are considered as spots",
            -G_MAXFLOAT, G_MAXFLOAT, 0.0f,
            G_PARAM_READWRITE);

    properties[PROP_SPOT_THRESHOLD_MODE] =
        g_param_spec_enum ("spot-threshold-mode",
            "Pixels must be either \"below\", \"above\" the spot threshold, or their \"absolute\" value can be compared",
            "Pixels must be either \"below\", \"above\" the spot threshold, or their \"absolute\" value can be compared",
            g_enum_register_static ("ufo_fls_spot_threshold_mode", spot_threshold_mode_values),
            SPOT_THRESHOLD_ABSOLUTE, G_PARAM_READWRITE);

    properties[PROP_GROW_THRESHOLD] =
        g_param_spec_float ("grow-threshold",
            "Spot growing threshold",
            "Spot growing threshold",
            0.0f, G_MAXFLOAT, 0.0f,
            G_PARAM_READWRITE);

    properties[PROP_ADDRESSING_MODE] =
        g_param_spec_enum ("addressing-mode",
            "Outlier treatment (\"none\", \"clamp\", \"clamp_to_edge\", \"repeat\", \"mirrored_repeat\")",
            "Outlier treatment (\"none\", \"clamp\", \"clamp_to_edge\", \"repeat\", \"mirrored_repeat\")",
            g_enum_register_static ("ufo_fls_addressing_mode", addressing_values),
            CL_ADDRESS_MIRRORED_REPEAT,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoFindLargeSpotsTaskPrivate));
}

static void
ufo_find_large_spots_task_init(UfoFindLargeSpotsTask *self)
{
    self->priv = UFO_FIND_LARGE_SPOTS_TASK_GET_PRIVATE(self);
    self->priv->spot_threshold = 0.0f;
    self->priv->grow_threshold = 0.0f;
    self->priv->spot_threshold_mode = SPOT_THRESHOLD_ABSOLUTE;
    self->priv->addressing_mode = CL_ADDRESS_MIRRORED_REPEAT;
}
