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
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "ufo-retrieve-phase-task.h"

#define IS_POW_OF_2(x) !(x & (x - 1))

typedef enum {
    METHOD_TIE = 0,
    METHOD_CTF,
    METHOD_CTF_MULTI,
    METHOD_QP,
    METHOD_QP2,
    N_METHODS
} Method;

static GEnumValue method_values[] = {
    { METHOD_TIE,           "METHOD_TIE",           "tie" },
    { METHOD_CTF,           "METHOD_CTF",           "ctf" },
    { METHOD_CTF_MULTI,     "METHOD_CTF_MULTI",     "ctf_multidistance" },
    { METHOD_QP,            "METHOD_QP",            "qp" },
    { METHOD_QP2,           "METHOD_QP2",           "qp2" },
    { 0, NULL, NULL}
};

struct _UfoRetrievePhaseTaskPrivate {
    Method method;
    gfloat energy;
    GValueArray *distance;
    gfloat distance_x, distance_y;
    gfloat pixel_size;
    gfloat regularization_rate;
    gfloat binary_filter;
    gfloat frequency_cutoff;
    gboolean output_filter;

    gfloat prefac[2];
    cl_kernel *kernels;
    cl_kernel mult_by_value_kernel, ctf_multi_apply_dist_kernel;
    cl_context context;
    UfoBuffer *filter_buffer;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoRetrievePhaseTask, ufo_retrieve_phase_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_RETRIEVE_PHASE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_RETRIEVE_PHASE_TASK, UfoRetrievePhaseTaskPrivate))


enum {
    PROP_0,
    PROP_METHOD,
    PROP_ENERGY,
    PROP_DISTANCE,
    PROP_DISTANCE_X,
    PROP_DISTANCE_Y,
    PROP_PIXEL_SIZE,
    PROP_REGULARIZATION_RATE,
    PROP_BINARY_FILTER_THRESHOLDING,
    PROP_FREQUENCY_CUTOFF,
    PROP_OUTPUT_FILTER,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_retrieve_phase_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_RETRIEVE_PHASE_TASK, NULL));
}

static void
ufo_retrieve_phase_task_setup (UfoTask *task,
                               UfoResources *resources,
                               GError **error)
{
    UfoRetrievePhaseTaskPrivate *priv;
    gfloat lambda, tmp;

    priv = UFO_RETRIEVE_PHASE_TASK_GET_PRIVATE (task);
    priv->context = ufo_resources_get_context (resources);

    lambda = 6.62606896e-34 * 299792458 / (priv->energy * 1.60217733e-16);
    tmp = G_PI * lambda / (priv->pixel_size * priv->pixel_size);
    if (priv->distance_x != 0.0f && priv->distance_y != 0.0f) {
        priv->prefac[0] = tmp * priv->distance_x;
        priv->prefac[1] = tmp * priv->distance_y;
    } else if (priv->distance->n_values > 0) {
        priv->prefac[0] = priv->prefac[1] = tmp * g_value_get_double (g_value_array_get_nth (priv->distance, 0));
    } else {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "Either both, distance_x and distance_y must be non-zero, or distance must be specified");
        return;
    }
    if (priv->distance->n_values > 1 && priv->method != METHOD_CTF_MULTI) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "When multiple distances are speicified method must be set to \"ctf_multidistance\"");
        return;
    }

    priv->kernels[METHOD_TIE] = ufo_resources_get_kernel (resources, "phase-retrieval.cl", "tie_method", NULL, error);
    priv->kernels[METHOD_CTF] = ufo_resources_get_kernel (resources, "phase-retrieval.cl", "ctf_method", NULL, error);
    priv->kernels[METHOD_CTF_MULTI] = ufo_resources_get_kernel (resources, "phase-retrieval.cl", "ctf_multidistance_square", NULL, error);
    priv->kernels[METHOD_QP] = ufo_resources_get_kernel (resources, "phase-retrieval.cl", "qp_method", NULL, error);
    priv->kernels[METHOD_QP2] = ufo_resources_get_kernel (resources, "phase-retrieval.cl", "qp2_method", NULL, error);

    priv->mult_by_value_kernel = ufo_resources_get_kernel (resources, "phase-retrieval.cl", "mult_by_value", NULL, error);
    priv->ctf_multi_apply_dist_kernel = ufo_resources_get_kernel (resources, "phase-retrieval.cl", "ctf_multidistance_apply_distance", NULL, error);

    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext(priv->context), error);

    if (priv->filter_buffer == NULL) {
        UfoRequisition requisition;
        requisition.n_dims = 2;
        requisition.dims[0] = 1;
        requisition.dims[1] = 1;

        priv->filter_buffer = ufo_buffer_new(&requisition, priv->context);
    }

    for (int i = 0; i < N_METHODS; i++) {
        if (priv->kernels[i] != NULL) {
            UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel(priv->kernels[i]), error);
        }
    }

    if (priv->mult_by_value_kernel != NULL) {
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->mult_by_value_kernel), error);
    }

    if (priv->ctf_multi_apply_dist_kernel != NULL) {
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->ctf_multi_apply_dist_kernel), error);
    }
}

static void
ufo_retrieve_phase_task_get_requisition (UfoTask *task,
                                         UfoBuffer **inputs,
                                         UfoRequisition *requisition,
                                         GError **error)
{
    UfoRetrievePhaseTaskPrivate *priv;

    priv = UFO_RETRIEVE_PHASE_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], requisition);
    if (priv->output_filter) {
        requisition->dims[0] >>= 1;
    }

    if (!IS_POW_OF_2 (requisition->dims[0]) || !IS_POW_OF_2 (requisition->dims[1])) {
        g_set_error_literal (error, UFO_TASK_ERROR, UFO_TASK_ERROR_GET_REQUISITION,
                             "Please, perform zeropadding of your dataset along both directions (width, height) up to length of power of 2 (e.g. 256, 512, 1024, 2048, etc.)");
    }
}

static guint
ufo_filter_task_get_num_inputs (UfoTask *task)
{
    UfoRetrievePhaseTaskPrivate *priv = UFO_RETRIEVE_PHASE_TASK_GET_PRIVATE (task);

    return MAX(1, priv->distance->n_values);
}

static guint
ufo_filter_task_get_num_dimensions (UfoTask *task, guint input)
{
    return 2;
}

static UfoTaskMode
ufo_filter_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_retrieve_phase_task_process (UfoTask *task,
                                 UfoBuffer **inputs,
                                 UfoBuffer *output,
                                 UfoRequisition *requisition)
{
    UfoRetrievePhaseTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    gsize global_work_size[3];
    cl_int cl_err;
    guint i;
    gfloat *distances, lambda, distance;
    gfloat fill_pattern = 0.0f;

    cl_mem current_in_mem, in_sum_mem, in_mem, out_mem, filter_mem, distances_mem;
    cl_kernel method_kernel;
    cl_command_queue cmd_queue;

    priv = UFO_RETRIEVE_PHASE_TASK_GET_PRIVATE (task);

    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));

    global_work_size[0] = requisition->dims[0];
    global_work_size[1] = requisition->dims[1];
    if (!priv->output_filter) {
        /* Filter is real as opposed to the complex input, so the width is only half of the interleaved input */
        global_work_size[0] >>= 1;
    }

    if (ufo_buffer_cmp_dimensions (priv->filter_buffer, requisition) != 0) {
        ufo_buffer_resize (priv->filter_buffer, requisition);
        filter_mem = ufo_buffer_get_device_array (priv->filter_buffer, cmd_queue);

        method_kernel = priv->kernels[(gint)priv->method];

        if (priv->method == METHOD_CTF_MULTI) {
            lambda = 6.62606896e-34 * 299792458 / (priv->energy * 1.60217733e-16);
            distances = g_malloc0 (priv->distance->n_values * sizeof (gfloat));
            for (i = 0; i < priv->distance->n_values; i++) {
                distances[i] = g_value_get_double (g_value_array_get_nth (priv->distance, i));
            }
            distances_mem = clCreateBuffer (priv->context,
                                           CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                           priv->distance->n_values * sizeof(float),
                                           distances,
                                           &cl_err);
            UFO_RESOURCES_CHECK_CLERR (cl_err);
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (method_kernel, 0, sizeof (cl_mem), &distances_mem));
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (method_kernel, 1, sizeof (guint), &priv->distance->n_values));
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (method_kernel, 2, sizeof (gfloat), &lambda));
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (method_kernel, 3, sizeof (gfloat), &priv->pixel_size));
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (method_kernel, 4, sizeof (gfloat), &priv->regularization_rate));
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (method_kernel, 5, sizeof (cl_mem), &filter_mem));
            g_free (distances);
        } else {
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (method_kernel, 0, sizeof (cl_float2), &priv->prefac));
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (method_kernel, 1, sizeof (gfloat), &priv->regularization_rate));
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (method_kernel, 2, sizeof (gfloat), &priv->binary_filter));
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (method_kernel, 3, sizeof (gfloat), &priv->frequency_cutoff));
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (method_kernel, 4, sizeof (cl_mem), &filter_mem));
        }
        ufo_profiler_call (profiler, cmd_queue, method_kernel, requisition->n_dims, global_work_size, NULL);
        if (priv->method == METHOD_CTF_MULTI) {
            UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (distances_mem));
        }
    }
    else {
        filter_mem = ufo_buffer_get_device_array (priv->filter_buffer, cmd_queue);
    }

    if (priv->output_filter) {
        ufo_buffer_copy (priv->filter_buffer, output);
    }
    else {
        if (priv->method == METHOD_CTF_MULTI) {
            /* Sum input frequencies first, then proceed wth complex division */
            in_sum_mem = clCreateBuffer (priv->context,
                                         CL_MEM_READ_WRITE,
                                         requisition->dims[0] * requisition->dims[1] * sizeof(float),
                                         NULL,
                                         &cl_err);
            UFO_RESOURCES_CHECK_CLERR (cl_err);
            UFO_RESOURCES_CHECK_CLERR (clEnqueueFillBuffer (cmd_queue, in_sum_mem, &fill_pattern, sizeof (cl_float),
                                                            0, requisition->dims[0] * requisition->dims[1] * sizeof(float),
                                                            0, NULL, NULL));
            for (i = 0; i < priv->distance->n_values; i++) {
                distance = g_value_get_double (g_value_array_get_nth (priv->distance, i));
                current_in_mem = ufo_buffer_get_device_array (inputs[i], cmd_queue);
                UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->ctf_multi_apply_dist_kernel, 0, sizeof (cl_mem), &current_in_mem));
                UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->ctf_multi_apply_dist_kernel, 1, sizeof (gfloat), &distance));
                UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->ctf_multi_apply_dist_kernel, 2, sizeof (guint), &priv->distance->n_values));
                UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->ctf_multi_apply_dist_kernel, 3, sizeof (gfloat), &lambda));
                UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->ctf_multi_apply_dist_kernel, 4, sizeof (gfloat), &priv->pixel_size));
                UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->ctf_multi_apply_dist_kernel, 5, sizeof (gfloat), &priv->regularization_rate));
                UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->ctf_multi_apply_dist_kernel, 6, sizeof (cl_mem), &in_sum_mem));
                ufo_profiler_call (profiler, cmd_queue, priv->ctf_multi_apply_dist_kernel, requisition->n_dims, global_work_size, NULL);
            }
            in_mem = in_sum_mem;
        } else {
            in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
        }

        out_mem = ufo_buffer_get_device_array (output, cmd_queue);
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->mult_by_value_kernel, 0, sizeof (cl_mem), &in_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->mult_by_value_kernel, 1, sizeof (cl_mem), &filter_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->mult_by_value_kernel, 2, sizeof (cl_mem), &out_mem));
        ufo_profiler_call (profiler, cmd_queue, priv->mult_by_value_kernel, requisition->n_dims, requisition->dims, NULL);

        if (priv->method == METHOD_CTF_MULTI) {
            UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (in_sum_mem));
        }
    }
    
    return TRUE;
}

static void
ufo_retrieve_phase_task_get_property (GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
    UfoRetrievePhaseTaskPrivate *priv = UFO_RETRIEVE_PHASE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_METHOD:
            g_value_set_enum (value, priv->method);
            break;
        case PROP_ENERGY:
            g_value_set_float (value, priv->energy);
            break;
        case PROP_DISTANCE:
            g_value_set_boxed (value, priv->distance);
            break;
        case PROP_DISTANCE_X:
            g_value_set_float (value, priv->distance_x);
            break;
        case PROP_DISTANCE_Y:
            g_value_set_float (value, priv->distance_y);
            break;
        case PROP_PIXEL_SIZE:
            g_value_set_float (value, priv->pixel_size);
            break;
        case PROP_REGULARIZATION_RATE:
            g_value_set_float (value, priv->regularization_rate);
            break;
        case PROP_BINARY_FILTER_THRESHOLDING:
            g_value_set_float (value, priv->binary_filter);
            break;
        case PROP_FREQUENCY_CUTOFF:
            g_value_set_float (value, priv->frequency_cutoff);
            break;
        case PROP_OUTPUT_FILTER:
            g_value_set_boolean (value, priv->output_filter);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_retrieve_phase_task_set_property (GObject *object,
                                      guint property_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
    UfoRetrievePhaseTaskPrivate *priv = UFO_RETRIEVE_PHASE_TASK_GET_PRIVATE (object);
    GValueArray *array;

    switch (property_id) {
        case PROP_METHOD:
            priv->method = g_value_get_enum (value);
            break;
        case PROP_ENERGY:
            priv->energy = g_value_get_float (value);
            break;
        case PROP_DISTANCE:
            array = (GValueArray *) g_value_get_boxed (value);
            if (array) {
                g_value_array_free (priv->distance);
                priv->distance = g_value_array_copy (array);
            }
            break;
        case PROP_DISTANCE_X:
            priv->distance_x = g_value_get_float (value);
            break;
        case PROP_DISTANCE_Y:
            priv->distance_y = g_value_get_float (value);
            break;
        case PROP_PIXEL_SIZE:
            priv->pixel_size = g_value_get_float (value);
            break;
        case PROP_REGULARIZATION_RATE:
            priv->regularization_rate = g_value_get_float (value);
            break;
        case PROP_BINARY_FILTER_THRESHOLDING:
            priv->binary_filter = g_value_get_float (value);
            break;
        case PROP_FREQUENCY_CUTOFF:
            priv->frequency_cutoff = g_value_get_float (value);
            break;
        case PROP_OUTPUT_FILTER:
            priv->output_filter = g_value_get_boolean (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }

}

static void
ufo_retrieve_phase_task_finalize (GObject *object)
{
    UfoRetrievePhaseTaskPrivate *priv;

    priv = UFO_RETRIEVE_PHASE_TASK_GET_PRIVATE (object);

    if (priv->kernels) {
        for (int i = 0; i < N_METHODS; i++) {
            if (priv->kernels[i]) {
                UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernels[i]));
                priv->kernels[i] = NULL;
            }
        }
    }

    g_free(priv->kernels);

    if (priv->mult_by_value_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->mult_by_value_kernel));
        priv->mult_by_value_kernel = NULL;
    }

    if (priv->ctf_multi_apply_dist_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->ctf_multi_apply_dist_kernel));
        priv->ctf_multi_apply_dist_kernel = NULL;
    }

    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }

    if (priv->filter_buffer) {
        g_object_unref(priv->filter_buffer);
    }

    g_value_array_free (priv->distance);

    G_OBJECT_CLASS (ufo_retrieve_phase_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_retrieve_phase_task_setup;
    iface->get_requisition = ufo_retrieve_phase_task_get_requisition;
    iface->get_num_inputs = ufo_filter_task_get_num_inputs;
    iface->get_num_dimensions = ufo_filter_task_get_num_dimensions;
    iface->get_mode = ufo_filter_task_get_mode;
    iface->process = ufo_retrieve_phase_task_process;
}

static void
ufo_retrieve_phase_task_class_init (UfoRetrievePhaseTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_retrieve_phase_task_set_property;
    gobject_class->get_property = ufo_retrieve_phase_task_get_property;
    gobject_class->finalize = ufo_retrieve_phase_task_finalize;

    GParamSpec *double_region_vals = g_param_spec_double ("double-region-values",
                                                          "Double Region values",
                                                          "Elements in double regions",
                                                          -INFINITY,
                                                          INFINITY,
                                                          0.0,
                                                          G_PARAM_READWRITE);

    properties[PROP_METHOD] =
        g_param_spec_enum ("method",
            "Method name",
            "Method name",
            g_enum_register_static ("ufo_retrieve_phase_method", method_values),
            METHOD_TIE,
            G_PARAM_READWRITE);

    properties[PROP_ENERGY] =
        g_param_spec_float ("energy",
            "Energy value",
            "Energy value.",
            0, G_MAXFLOAT, 20,
            G_PARAM_READWRITE);

    properties[PROP_DISTANCE] =
        g_param_spec_value_array ("distance",
            "Distance value",
            "Distance value.",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_DISTANCE_X] =
        g_param_spec_float ("distance-x",
            "Distance value for x-direction",
            "Distance value for x-direction",
            0, G_MAXFLOAT, 0.0f,
            G_PARAM_READWRITE);

    properties[PROP_DISTANCE_Y] =
        g_param_spec_float ("distance-y",
            "Distance value for y-direction",
            "Distance value for y-direction",
            0, G_MAXFLOAT, 0.0f,
            G_PARAM_READWRITE);

    properties[PROP_PIXEL_SIZE] =
        g_param_spec_float ("pixel-size",
            "Pixel size",
            "Pixel size.",
            0, G_MAXFLOAT, 0.75e-6,
            G_PARAM_READWRITE);

    properties[PROP_REGULARIZATION_RATE] =
        g_param_spec_float ("regularization-rate",
            "Regularization rate value",
            "Regularization rate value.",
            0, G_MAXFLOAT, 2.5,
            G_PARAM_READWRITE);

    properties[PROP_BINARY_FILTER_THRESHOLDING] =
        g_param_spec_float ("thresholding-rate",
            "Binary thresholding rate value",
            "Binary thresholding rate value.",
            0, G_MAXFLOAT, 0.1,
            G_PARAM_READWRITE);

    properties[PROP_FREQUENCY_CUTOFF] =
        g_param_spec_float ("frequency-cutoff",
            "Cut-off frequency in radians",
            "Cut-off frequency in radians",
            0, G_MAXFLOAT, G_MAXFLOAT,
            G_PARAM_READWRITE);

    properties[PROP_OUTPUT_FILTER] =
        g_param_spec_boolean ("output-filter",
            "Output the frequency filter, not the result of the filtering",
            "Output the frequency filter, not the result of the filtering",
            FALSE,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoRetrievePhaseTaskPrivate));
}

static void
ufo_retrieve_phase_task_init(UfoRetrievePhaseTask *self)
{
    UfoRetrievePhaseTaskPrivate *priv;

    self->priv = priv = UFO_RETRIEVE_PHASE_TASK_GET_PRIVATE(self);
    priv->method = METHOD_TIE;
    priv->energy = 20.0f;
    priv->distance = g_value_array_new (1);
    priv->distance_x = 0.0f;
    priv->distance_y = 0.0f;
    priv->pixel_size = 0.75e-6f;
    priv->regularization_rate = 2.5f;
    priv->binary_filter = 0.1f;
    priv->frequency_cutoff = G_MAXFLOAT;
    priv->kernels = (cl_kernel *) g_malloc0(N_METHODS * sizeof(cl_kernel));
    priv->filter_buffer = NULL;
    priv->output_filter = FALSE;
}
