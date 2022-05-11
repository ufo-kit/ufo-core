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

#include <string.h>
#include "ufo-correlate-stacks-task.h"


#define USE_GPU  0


struct _UfoCorrelateStacksTaskPrivate {
    guint number;
    gsize num_references;
    guint current;
    gboolean generated;
#if USE_GPU
    cl_context context;
    cl_mem result;
    cl_kernel diff_kernel;
    cl_kernel sum_kernel;
#endif
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoCorrelateStacksTask, ufo_correlate_stacks_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_CORRELATE_STACKS_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CORRELATE_STACKS_TASK, UfoCorrelateStacksTaskPrivate))

enum {
    PROP_0,
    PROP_NUMBER,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_correlate_stacks_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_CORRELATE_STACKS_TASK, NULL));
}

static void
ufo_correlate_stacks_task_setup (UfoTask *task,
                                 UfoResources *resources,
                                 GError **error)
{
    UfoCorrelateStacksTaskPrivate *priv;

    priv = UFO_CORRELATE_STACKS_TASK_GET_PRIVATE (task);

    if (priv->number == 0) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "::number not set");
        return;
    }

#if USE_GPU
    priv->diff_kernel = ufo_resources_get_kernel (resources, "correlate.cl", "diff", error);
    priv->sum_kernel = ufo_resources_get_kernel (resources, "correlate.cl", "sum", error);

    if (priv->diff_kernel != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->diff_kernel), error);

    if (priv->sum_kernel != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->sum_kernel), error);

    priv->context = ufo_resources_get_context (resources);
    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);
#endif

    priv->current = 0;
    priv->generated = FALSE;
}

static void
ufo_correlate_stacks_task_get_requisition (UfoTask *task,
                                           UfoBuffer **inputs,
                                           UfoRequisition *requisition,
                                           GError **error)
{
    UfoCorrelateStacksTaskPrivate *priv;
    UfoRequisition ref_req;

    priv = UFO_CORRELATE_STACKS_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], &ref_req);
    priv->num_references = ref_req.dims[2];

#if USE_GPU
    if (priv->result == NULL) {
        cl_int error;

        priv->result = clCreateBuffer (priv->context, CL_MEM_READ_WRITE, ufo_buffer_get_size (inputs[0]), NULL, &error);
        UFO_RESOURCES_CHECK_CLERR (error);
    }
#endif

    /* Output is a correlation matrix with rows being input and columns
     * references. */
    requisition->n_dims = 2;
    requisition->dims[0] = priv->num_references;
    requisition->dims[1] = priv->number;
}

static guint
ufo_correlate_stacks_task_get_num_inputs (UfoTask *task)
{
    return 2;
}

static guint
ufo_correlate_stacks_task_get_num_dimensions (UfoTask *task,
                                              guint input)
{
    if (input == 0)
        return 3;

    return 2;
}

static UfoTaskMode
ufo_correlate_stacks_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_REDUCTOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_correlate_stacks_task_process (UfoTask *task,
                                   UfoBuffer **inputs,
                                   UfoBuffer *output,
                                   UfoRequisition *requisition)
{
    UfoCorrelateStacksTaskPrivate *priv;
    UfoRequisition refs_req;
#if USE_GPU
    UfoProfiler *profiler;
    UfoGpuNode *node;
    cl_command_queue queue;
    cl_mem in_mem;
    cl_mem ref_mem;
    cl_mem matrix_mem;
    gsize work_size[2];
    guint width;
    guint height;
#else
    UfoRequisition in_req;
#endif

    priv = UFO_CORRELATE_STACKS_TASK_GET_PRIVATE (task);

    if (priv->current >= priv->number) {
        g_warning ("Received too many inputs");
        return FALSE;
    }

    ufo_buffer_get_requisition (inputs[0], &refs_req);

#if USE_GPU
    width = refs_req.dims[0];
    height = refs_req.dims[1];
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    queue = ufo_gpu_node_get_cmd_queue (node);
    ref_mem = ufo_buffer_get_device_array (inputs[0], queue);
    in_mem = ufo_buffer_get_device_array (inputs[1], queue);

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->diff_kernel, 0, sizeof (cl_mem), &ref_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->diff_kernel, 1, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->diff_kernel, 2, sizeof (cl_mem), &priv->result));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->diff_kernel, 3, sizeof (guint), &height));

    work_size[0] = refs_req.dims[0];
    work_size[1] = refs_req.dims[1] * refs_req.dims[2];

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, queue, priv->diff_kernel, 2, work_size, NULL);

    matrix_mem = ufo_buffer_get_device_array (output, queue);

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->sum_kernel, 0, sizeof (cl_mem), &priv->result));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->sum_kernel, 1, sizeof (cl_mem), &matrix_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->sum_kernel, 2, sizeof (guint), &width));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->sum_kernel, 3, sizeof (guint), &height));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->sum_kernel, 4, sizeof (guint), &priv->current));

    work_size[0] = requisition->dims[0];
    ufo_profiler_call (profiler, queue, priv->sum_kernel, 1, work_size, NULL);
#else
    gfloat *refs;
    gfloat *in_mem;
    gfloat *out_mem;
    refs = ufo_buffer_get_host_array (inputs[0], NULL);
    in_mem = ufo_buffer_get_host_array (inputs[1], NULL);
    out_mem = ufo_buffer_get_host_array (output, NULL);

    ufo_buffer_get_requisition (inputs[1], &in_req);

    for (gsize i = 0; i < refs_req.dims[2]; i++) {
        gfloat *ref;
        gfloat sum = 0;

        ref = refs + i * refs_req.dims[0] * refs_req.dims[1];

        for (gsize j = 0; j < in_req.dims[0] * in_req.dims[1]; j++) {
            sum += (ref[j] - in_mem[j]) * (ref[j] - in_mem[j]);
        }

        out_mem[i * priv->number + priv->current] = sum;
    }
#endif

    priv->current++;
    return TRUE;
}

static gboolean
ufo_correlate_stacks_task_generate (UfoTask *task,
                                    UfoBuffer *output,
                                    UfoRequisition *requisition)
{
    UfoCorrelateStacksTaskPrivate *priv;

    priv = UFO_CORRELATE_STACKS_TASK_GET_PRIVATE (task);

    if (priv->generated)
        return FALSE;

    priv->generated = TRUE;

    return TRUE;
}

static void
ufo_correlate_stacks_task_set_property (GObject *object,
                                        guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
    UfoCorrelateStacksTaskPrivate *priv = UFO_CORRELATE_STACKS_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUMBER:
            priv->number = g_value_get_uint (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_correlate_stacks_task_get_property (GObject *object,
                                        guint property_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
    UfoCorrelateStacksTaskPrivate *priv = UFO_CORRELATE_STACKS_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUMBER:
            g_value_set_uint (value, priv->number);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_correlate_stacks_task_finalize (GObject *object)
{
#if USE_GPU
    UfoCorrelateStacksTaskPrivate *priv;

    priv = UFO_CORRELATE_STACKS_TASK_GET_PRIVATE (object);

    if (priv->result) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->result));
        priv->result = NULL;
    }

    if (priv->diff_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->diff_kernel));
        priv->diff_kernel = NULL;
    }

    if (priv->sum_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->sum_kernel));
        priv->sum_kernel = NULL;
    }

    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }
#endif

    G_OBJECT_CLASS (ufo_correlate_stacks_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_correlate_stacks_task_setup;
    iface->get_num_inputs = ufo_correlate_stacks_task_get_num_inputs;
    iface->get_num_dimensions = ufo_correlate_stacks_task_get_num_dimensions;
    iface->get_mode = ufo_correlate_stacks_task_get_mode;
    iface->get_requisition = ufo_correlate_stacks_task_get_requisition;
    iface->process = ufo_correlate_stacks_task_process;
    iface->generate = ufo_correlate_stacks_task_generate;
}

static void
ufo_correlate_stacks_task_class_init (UfoCorrelateStacksTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_correlate_stacks_task_set_property;
    oclass->get_property = ufo_correlate_stacks_task_get_property;
    oclass->finalize = ufo_correlate_stacks_task_finalize;

    properties[PROP_NUMBER] =
        g_param_spec_uint ("number",
            "Number of input items",
            "Number of input items",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoCorrelateStacksTaskPrivate));
}

static void
ufo_correlate_stacks_task_init(UfoCorrelateStacksTask *self)
{
    self->priv = UFO_CORRELATE_STACKS_TASK_GET_PRIVATE(self);
    self->priv->number = 0;
    self->priv->num_references = 0;

#if USE_GPU
    self->priv->result = NULL;
    self->priv->diff_kernel = NULL;
    self->priv->sum_kernel = NULL;
    self->priv->context = NULL;
#endif
}
