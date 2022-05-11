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

#include <math.h>
#include <glib.h>
#include <glib/gprintf.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include "ufo-measure-task.h"

/**
 * SECTION:ufo-measure-task
 * @Short_description: Measure basic image properties with GSL
 * @Title: measure
 *
 */

typedef enum {
    M_MIN,
    M_MAX,
    M_SUM,
    M_MEAN,
    M_VAR,
    M_STD,
    M_SKEW,
    M_KURTOSIS,
    M_LAST
} Metric;

static GEnumValue metric_values[] = {
    { M_MIN,        "M_MIN",       "min"       },
    { M_MAX,        "M_MAX",       "max"       },
    { M_SUM,        "M_SUM",       "sum"       },
    { M_MEAN,       "M_SUM",       "mean"      },
    { M_VAR,        "M_SQUARE",    "var"       },
    { M_STD,        "M_SQUARE",    "std"       },
    { M_SKEW,       "M_CUBE",      "skew"      },
    { M_KURTOSIS,   "M_QUADRATE",  "kurtosis"  },
    {0, NULL, NULL}
};

/* Statistics often need some posprocessing (normalization). This table maps metric */
/* to the respective postprocessing OpenCL code */
static gchar *postproc_codes[] = {
    NULL,
    NULL,
    NULL,
    "array[x] * param_scalar",
    "array[x] * param_scalar",
    "sqrt (array[x] * param_scalar)",
    "param[x] != 0.0f ? array[x] * param_scalar / pow (param[x], 1.5f) : 0.0f",
    "(param[x] != 0.0f ? array[x] * param_scalar / (param[x] * param[x]) : 0.0f) - 3.0f",
};

struct _UfoMeasureTaskPrivate {
    size_t local_shape[2];
    cl_context context;
    /* Metrics often iteract, e.g. kurtosis needs mean and variance, thus use */
    /* separate kernels and memories so that they can be easily shared between */
    /* metrics. */
    cl_kernel kernels[M_LAST], postproc_kernels[M_LAST];
    cl_mem mems[M_LAST];
    Metric metric;
    gint axis;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoMeasureTask, ufo_measure_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_MEASURE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_MEASURE_TASK, UfoMeasureTaskPrivate))

enum {
    PROP_0,
    PROP_METRIC,
    PROP_AXIS,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_measure_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_MEASURE_TASK, NULL));
}

static gchar *
create_postprocessing_kernel (const gchar *exec_code)
{
    const gchar *template = "kernel void calculate (global float *array, "\
                            "global float *param, const float param_scalar) "\
                            "{int x = get_global_id (0); array[x] = ";

    return g_strconcat (template, exec_code, ";}", NULL);
}

static gint
next_power_of_two (gdouble value)
{
    return pow (2, (gint) ceil (log2 (value)));
}

static void
ufo_measure_task_setup (UfoTask *task,
                        UfoResources *resources,
                        GError **error)
{
    size_t local_size;
    GValue *local_size_gvalue;
    UfoMeasureTaskPrivate *priv = UFO_MEASURE_TASK_GET_PRIVATE (task);
    UfoGpuNode *node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    local_size_gvalue = ufo_gpu_node_get_info (node, UFO_GPU_NODE_INFO_MAX_WORK_GROUP_SIZE);
    local_size = g_value_get_ulong (local_size_gvalue);
    g_value_unset (local_size_gvalue);
    gchar *axis, *kernel_name, *postproc_source;
    gint i;

    if (priv->axis == -1) {
        priv->local_shape[0] = local_size;
        priv->local_shape[1] = 1;
        axis = g_strdup("");
    }
    else {
        if (priv->axis == 0) {
            priv->local_shape[0] = 128;
            priv->local_shape[1] = 1;
        } else {
            priv->local_shape[0] = 32;
            priv->local_shape[1] = 8;
        }
        axis = g_strdup_printf ("%d", priv->axis);
    }

    while (priv->local_shape[0] * priv->local_shape[1] > local_size) {
        priv->local_shape[0] /= 2;
    }

    g_debug ("Measure local work group size: %lu %lu", priv->local_shape[0], priv->local_shape[1]);

    for (i = 0; i < M_LAST; i++) {
        /* Reduction kernels */
        kernel_name = g_strconcat ("reduce_", axis, metric_values[i].value_name, NULL),
        priv->kernels[i] = ufo_resources_get_kernel (resources, "reductor.cl", kernel_name, NULL, error);
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernels[i]), error);

        /* Postprocessing (normalization) kernels */
        priv->postproc_kernels[i] = NULL;
        if (postproc_codes[i]) {
            postproc_source = create_postprocessing_kernel (postproc_codes[i]);
            priv->postproc_kernels[i] = ufo_resources_get_kernel_from_source (resources, postproc_source, "calculate", NULL, error);
            UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->postproc_kernels[i]), error);
            g_free (postproc_source);
        }

        priv->mems[i] = NULL;
    }

    priv->context = ufo_resources_get_context (resources);
    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);
    g_free (axis);
}

static gsize
get_input_size (UfoMeasureTaskPrivate *priv,
                 UfoBuffer *input)
{
    UfoRequisition requisition;

    ufo_buffer_get_requisition (input, &requisition);
    return priv->axis < 0 ? requisition.dims[0] * requisition.dims[1] : requisition.dims[priv->axis];
}

static gsize
get_output_size (UfoMeasureTaskPrivate *priv,
                UfoBuffer *input)
{
    UfoRequisition requisition;

    ufo_buffer_get_requisition (input, &requisition);
    return priv->axis < 0 ? 1 : requisition.dims[1 - priv->axis];
}

static void
ufo_measure_task_get_requisition (UfoTask *task,
                                  UfoBuffer **inputs,
                                  UfoRequisition *requisition,
                                  GError **error)
{
    UfoMeasureTaskPrivate *priv;
    UfoRequisition in_req;

    priv = UFO_MEASURE_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], &in_req);
    requisition->n_dims = in_req.n_dims - 1;
    requisition->dims[0] = get_output_size (priv, inputs[0]);
}

static guint
ufo_measure_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_measure_task_get_num_dimensions (UfoTask *task,
                                     guint input)
{
    return 2;
}

static UfoTaskMode
ufo_measure_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static cl_mem
create_output (UfoMeasureTaskPrivate *priv,
               UfoBuffer *input)
{
    cl_int error;
    cl_mem output;
    UfoRequisition requisition;
    gsize pixels_per_thread;
    size_t other_dim = 1;
    cl_ulong num_groups;

    ufo_buffer_get_requisition (input, &requisition);
    if (priv->axis >= 0) {
        other_dim = requisition.dims[1 - priv->axis];
        num_groups = requisition.dims[priv->axis];
    } else {
        num_groups = requisition.dims[0] * requisition.dims[1];
    }
    num_groups = (num_groups - 1) / priv->local_shape[priv->axis < 0 ? 0 : priv->axis] + 1;
    pixels_per_thread = MAX (32, next_power_of_two (sqrt (num_groups)));
    num_groups = (num_groups - 1) / pixels_per_thread + 1;
    g_debug ("Measure result memory size (dimensions order arbitrary): (%lu, %lu)", num_groups, other_dim);
    output = clCreateBuffer (priv->context,
                             CL_MEM_READ_WRITE,
                             num_groups * other_dim * sizeof (cl_float),
                             NULL,
                             &error);
    UFO_RESOURCES_CHECK_CLERR (error);

    return output;
}

static void copy_result (UfoTask *task,
                         cl_mem input,
                         UfoBuffer *output)
{
    UfoGpuNode *node;
    cl_command_queue cmd_queue;
    cl_mem output_mem;

    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    output_mem = ufo_buffer_get_device_array (output, cmd_queue);

    UFO_RESOURCES_CHECK_CLERR (clEnqueueCopyBuffer (cmd_queue,
                                                    input, output_mem,
                                                    0, 0,
                                                    ufo_buffer_get_size (output),
                                                    0, NULL, NULL));
}

static void
reduce (UfoTask *task,
        cl_kernel op_kernel,
        cl_kernel sum_kernel,
        UfoBuffer *input,
        cl_mem output,
        cl_mem param)
{
    UfoMeasureTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    UfoRequisition requisition;
    cl_command_queue cmd_queue;
    cl_kernel kernel;
    cl_ulong num_groups, real_size;
    cl_int pixels_per_thread, input_width, other_dim, real_shape[2], result_width;
    cl_mem in_mem;
    size_t exec_shape[2];
    gint axis;

    priv = UFO_MEASURE_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    in_mem = ufo_buffer_get_device_array (input, cmd_queue);
    ufo_buffer_get_requisition (input, &requisition);

    /* Balance the load and process multiple times until the global reduction
     * result is stored in the first pixel. One work item in the kernel
     * processes more pixels (global work size is thus less than the input
     * size). At the same time, we try to have many groups in order to have good
     * occupancy. */
    kernel = op_kernel;
    num_groups = priv->axis == -1 ? requisition.dims[0] * requisition.dims[1] : requisition.dims[priv->axis];
    input_width = requisition.dims[0];
    if (priv->axis < 0) {
        /* From now on if the axis is -1, treat the computation as horizontal */
        axis = 0;
        other_dim = 1;
    } else {
        axis = priv->axis;
        other_dim = requisition.dims[1 - axis];
    }

    while (TRUE) {
        /* Real shape is the previous number of groups and the other dimension */
        real_shape[axis] = num_groups;
        real_shape[1 - axis] = other_dim;
        /* Make sure global work size divides the local work size */
        /* Number of groups processing real size data */
        num_groups = (real_shape[axis] - 1) / priv->local_shape[axis] + 1;
        pixels_per_thread = MAX (32, next_power_of_two (sqrt (num_groups)));
        num_groups = (num_groups - 1) / pixels_per_thread + 1;
        exec_shape[axis] = num_groups * priv->local_shape[axis];
        /* Make *exec_shape* dividable by *local_shape* */
        exec_shape[1 - axis] = (real_shape[1 - axis] - 1) / priv->local_shape[1 - axis] + 1;
        exec_shape[1 - axis] *= priv->local_shape[1 - axis];
        /* Result width is only needed for the horizontal kernel and in that
         * case it is the number of groups */
        result_width = num_groups;

        g_debug ("Measure real size: (%d, %d) global size: (%lu, %lu) in_width: %d res_width: %d G: %lu PPT: %d",
                 real_shape[0], real_shape[1], exec_shape[0], exec_shape[1], input_width, result_width,
                 num_groups, pixels_per_thread);
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 0, sizeof (cl_mem), &in_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 1, sizeof (cl_mem), &output));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 2, sizeof (cl_mem), &param));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 3, priv->local_shape[0] * priv->local_shape[1] *
                                                   sizeof (cl_float), NULL));
        if (priv->axis < 0) {
            real_size = real_shape[0];
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 4, sizeof (cl_ulong), &real_size));
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 5, sizeof (cl_int), &pixels_per_thread));
            ufo_profiler_call (profiler, cmd_queue, kernel, 1, exec_shape, priv->local_shape);
        } else {
            /* We need real shape because the global kernel dimensions need to
             * be able to divide the chosen local shape, then we need to know
             * input width because input can be either the original image or an
             * intermediate result from the second iteration on. Result width is
             * the number of horizontal groups in case of horizontal reduction
             * and changes every iteration. */
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 4, sizeof (cl_int), &real_shape[0]));
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 5, sizeof (cl_int), &real_shape[1]));
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 6, sizeof (cl_int), &input_width));
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 7, sizeof (cl_int), &result_width));
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 8, sizeof (cl_int), &pixels_per_thread));
            ufo_profiler_call (profiler, cmd_queue, kernel, 2, exec_shape, priv->local_shape);
        }
        if (num_groups == 1) {
            break;
        }

        /* Result becomes input, we need only the summing kernel from now on and
         * we need to adjust the input_width as well. */
        in_mem = output;
        kernel = sum_kernel;
        if (priv->axis == 0 && input_width == (gint) requisition.dims[0]) {
            input_width = result_width;
        }
    }
}

static void
execute_postproc_kernel (UfoTask *task,
                         cl_kernel kernel,
                         cl_mem mem,
                         cl_mem param_mem,
                         cl_float param_scalar)
{
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    gsize global_work_size;

    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    UFO_RESOURCES_CHECK_CLERR (clGetMemObjectInfo (mem, CL_MEM_SIZE, sizeof (size_t),
                                                   &global_work_size, NULL));
    global_work_size /= sizeof (cl_float);

    if (!param_mem) {
        /* If *param_scalar* is NULL, the parameter is not needed. Assign the
         * output memory as a placeholder. */
        param_mem = mem;
    }

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 0, sizeof (cl_mem), &mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 1, sizeof (cl_mem), &param_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 2, sizeof (cl_float), &param_scalar));
    ufo_profiler_call (profiler, cmd_queue, kernel, 1, &global_work_size, NULL);
}

/* Generic reduction. Data is reduced based on reduction operation defined by
 * *metric*, *sum_kernel* defines how to proceed with values in the shared
 * memory. For trivial reductions, like min or sum, this is the same operation.
 * For reductions which apply some operation this is typically sum.
 * *reduction_param_mem* is the parameter which is applied during the reduction
 * process (e.g. mean sutraction by var). *postproc_param_mem* is the parameter
 * memory used for postprocessing (e.g. var by kurtosis).
 */
static void
_compute_default (UfoTask *task,
                 UfoBuffer *input,
                 UfoBuffer *output,
                 Metric metric,
                 cl_kernel sum_kernel,
                 cl_mem reduction_param_mem,
                 cl_mem postproc_param_mem)
{
    UfoMeasureTaskPrivate *priv = UFO_MEASURE_TASK_GET_PRIVATE (task);

    if (!priv->mems[metric]) {
        priv->mems[metric] = create_output (priv, input);
    }
    if (!reduction_param_mem) {
        /* param not needed, use the result memory for uniform computations. */
        reduction_param_mem = priv->mems[metric];
    }
    if (!postproc_param_mem) {
        /* Same for the postprocessing parameter */
        postproc_param_mem = priv->mems[metric];
    }
    if (!sum_kernel) {
        sum_kernel = priv->kernels[metric];
    }

    reduce (task, priv->kernels[metric], sum_kernel, input, priv->mems[metric], reduction_param_mem);
    if (priv->postproc_kernels[metric]) {
        execute_postproc_kernel (task, priv->postproc_kernels[metric], priv->mems[metric],
                                 postproc_param_mem, 1.0f / get_input_size (priv, input));
    }

    if (output) {
        copy_result (task, priv->mems[metric], output);
    }
}

static void
compute_default (UfoTask *task,
                UfoBuffer *input,
                UfoBuffer *output)
{
    UfoMeasureTaskPrivate *priv = UFO_MEASURE_TASK_GET_PRIVATE (task);
    _compute_default (task, input, output, priv->metric, NULL, NULL, NULL);
}

static void
compute_variance (UfoTask *task,
                  UfoBuffer *input,
                  UfoBuffer *output)
{
    UfoMeasureTaskPrivate *priv = UFO_MEASURE_TASK_GET_PRIVATE (task);
    _compute_default (task, input, NULL, M_MEAN, NULL, NULL, NULL);
    _compute_default (task, input, output, M_VAR, priv->kernels[M_SUM], priv->mems[M_MEAN], NULL);
}

static void
compute_std (UfoTask *task,
             UfoBuffer *input,
             UfoBuffer *output)
{
    UfoMeasureTaskPrivate *priv = UFO_MEASURE_TASK_GET_PRIVATE (task);
    _compute_default (task, input, NULL, M_MEAN, NULL, NULL, NULL);
    _compute_default (task, input, output, M_STD, priv->kernels[M_SUM], priv->mems[M_MEAN], NULL);
}

/* Skew and kurtosis both use priv->metric for postprocessing, thus the correct
 * function is invoked and there is no need to have separate functions for both
 * metrics
 */
static void
compute_skew_kurtosis (UfoTask *task,
                       UfoBuffer *input,
                       UfoBuffer *output)
{
    UfoMeasureTaskPrivate *priv = UFO_MEASURE_TASK_GET_PRIVATE (task);
    compute_variance (task, input, NULL);
    _compute_default (task, input, output, priv->metric, priv->kernels[M_SUM],
                      priv->mems[M_MEAN], priv->mems[M_VAR]);
}

static gboolean
ufo_measure_task_process (UfoTask *task,
                          UfoBuffer **inputs,
                          UfoBuffer *output,
                          UfoRequisition *requisition)
{
    UfoMeasureTaskPrivate *priv;
    typedef void (*MetricFunc) (UfoTask *, UfoBuffer *, UfoBuffer *);
    MetricFunc metric_func[] = {compute_default, compute_default, compute_default,
                                compute_default, compute_variance, compute_std,
                                compute_skew_kurtosis, compute_skew_kurtosis};

    priv = UFO_MEASURE_TASK_GET_PRIVATE (task);
    metric_func[priv->metric] (task, inputs[0], output);

    return TRUE;
}

static void
ufo_measure_task_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
    UfoMeasureTaskPrivate *priv;

    priv = UFO_MEASURE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_AXIS:
            priv->axis = g_value_get_int (value);
            break;

        case PROP_METRIC:
            priv->metric = g_value_get_enum (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_measure_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoMeasureTaskPrivate *priv = UFO_MEASURE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_AXIS:
            g_value_set_int (value, priv->axis);
            break;
        case PROP_METRIC:
            g_value_set_enum (value, priv->metric);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_measure_task_finalize (GObject *object)
{
    UfoMeasureTaskPrivate *priv = UFO_MEASURE_TASK_GET_PRIVATE (object);
    gint i;

    for (i = 0; i < M_LAST; i++) {
        if (priv->kernels[i]) {
            UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernels[i]));
            priv->kernels[i] = NULL;
        }
        if (priv->postproc_kernels[i]) {
            UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->postproc_kernels[i]));
            priv->postproc_kernels[i] = NULL;
        }
        if (priv->mems[i]) {
            UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->mems[i]));
            priv->mems[i] = NULL;
        }
    }

    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }

    G_OBJECT_CLASS (ufo_measure_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_measure_task_setup;
    iface->get_num_inputs = ufo_measure_task_get_num_inputs;
    iface->get_num_dimensions = ufo_measure_task_get_num_dimensions;
    iface->get_mode = ufo_measure_task_get_mode;
    iface->get_requisition = ufo_measure_task_get_requisition;
    iface->process = ufo_measure_task_process;
}

static void
ufo_measure_task_class_init (UfoMeasureTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_measure_task_set_property;
    oclass->get_property = ufo_measure_task_get_property;
    oclass->finalize = ufo_measure_task_finalize;

    properties[PROP_METRIC] =
        g_param_spec_enum ("metric",
            "Metric (min, max, sum, mean, var, std, skew, kurtosis)",
            "Metric (min, max, sum, mean, var, std, skew, kurtosis)",
            g_enum_register_static ("ufo_measure_metric", metric_values),
            M_STD, G_PARAM_READWRITE);

    properties[PROP_AXIS] =
        g_param_spec_int ("axis",
            "Along which axis to measure (-1, all)",
            "Along which axis to measure (-1, all)",
            -1, UFO_BUFFER_MAX_NDIMS, -1,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoMeasureTaskPrivate));
}

static void
ufo_measure_task_init(UfoMeasureTask *self)
{
    self->priv = UFO_MEASURE_TASK_GET_PRIVATE(self);
    self->priv->axis = -1;
    self->priv->metric = M_STD;
}
