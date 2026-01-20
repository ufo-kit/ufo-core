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
#include <math.h>

#include "ufo-filter-stripes1d-task.h"


struct _UfoFilterStripes1dTaskPrivate {
    guint last_width;

    /* OpenCL */
    cl_context context;
    cl_kernel kernel;
    cl_mem filter_mem;

    /* properties */
    gfloat strength;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoFilterStripes1dTask, ufo_filter_stripes1d_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_FILTER_STRIPES1D_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_STRIPES1D_TASK, UfoFilterStripes1dTaskPrivate))

enum {
    PROP_0,
    PROP_STRENGTH,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

/**
 * create_coefficients:
 * @priv: UfoFilterStripes1dTaskPrivate
 * @width: image width (not the interleaved from fft, rather the real width)
 *
 * Compute symmetric filter coefficients.
 */
static void
create_coefficients (UfoFilterStripes1dTaskPrivate *priv, const guint width)
{
    gfloat *coefficients = g_malloc0 (sizeof (gfloat) * width);
    gfloat sigma = priv->strength / (2.0 * sqrt (2.0 * log (2.0)));
    /* Divided by 2 because of the symmetry and we need the +1 to correctly handle
     * the frequency at width / 2 */
    const guint real_width = width / 2 + 1;
    cl_int cl_err;

    if (coefficients == NULL) {
        g_warning ("Could not allocate memory for coeefficients");
        return;
    }

    if (width % 2) {
        g_warning ("Width must be an even number");
    }

    for (gint x = 0; x < (gint) real_width; x++) {
        coefficients[x] = 1.0f - exp (- x * x / (sigma * sigma * 2.0f));
    }

    if (priv->filter_mem) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->filter_mem));
    }
    priv->filter_mem = clCreateBuffer (priv->context,
                                       CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                       sizeof (cl_float) * real_width,
                                       coefficients,
                                       &cl_err);
    UFO_RESOURCES_CHECK_CLERR (cl_err);
    g_free (coefficients);
    priv->last_width = width;
}

UfoNode *
ufo_filter_stripes1d_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_FILTER_STRIPES1D_TASK, NULL));
}

static void
ufo_filter_stripes1d_task_setup (UfoTask *task,
                                 UfoResources *resources,
                                 GError **error)
{
    UfoFilterStripes1dTaskPrivate *priv;

    priv = UFO_FILTER_STRIPES1D_TASK_GET_PRIVATE (task);
    priv->context = ufo_resources_get_context (resources);
    priv->kernel = ufo_resources_get_kernel (resources, "complex.cl", "c_mul_real_sym", NULL, error);
    priv->filter_mem = NULL;

    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);

    if (priv->kernel)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
}

static void
ufo_filter_stripes1d_task_get_requisition (UfoTask *task,
                                           UfoBuffer **inputs,
                                           UfoRequisition *requisition,
                                           GError **error)
{
    UfoFilterStripes1dTaskPrivate *priv;

    priv = UFO_FILTER_STRIPES1D_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], requisition);

    if (!priv->filter_mem || requisition->dims[0] / 2 != priv->last_width)
        create_coefficients (priv, requisition->dims[0] / 2);
}

static guint
ufo_filter_stripes1d_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_filter_stripes1d_task_get_num_dimensions (UfoTask *task,
                                              guint input)
{
    g_return_val_if_fail (input == 0, 0);

    return 2;
}

static UfoTaskMode
ufo_filter_stripes1d_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_filter_stripes1d_task_process (UfoTask *task,
                                   UfoBuffer **inputs,
                                   UfoBuffer *output,
                                   UfoRequisition *requisition)
{
    UfoFilterStripes1dTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    cl_mem in_mem;
    cl_mem out_mem;

    priv = UFO_FILTER_STRIPES1D_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (cl_mem), &priv->filter_mem));

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, requisition->dims, NULL);

    return TRUE;
}

static void
ufo_filter_stripes1d_task_set_property (GObject *object,
                                        guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
    UfoFilterStripes1dTaskPrivate *priv = UFO_FILTER_STRIPES1D_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_STRENGTH:
            priv->strength = g_value_get_float (value);
            if (priv->last_width) {
                /* Update only if we know how big the data is */
                create_coefficients (priv, priv->last_width);
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_filter_stripes1d_task_get_property (GObject *object,
                                        guint property_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
    UfoFilterStripes1dTaskPrivate *priv = UFO_FILTER_STRIPES1D_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_STRENGTH:
            g_value_set_float (value, priv->strength);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_filter_stripes1d_task_finalize (GObject *object)
{
    UfoFilterStripes1dTaskPrivate *priv;

    priv = UFO_FILTER_STRIPES1D_TASK_GET_PRIVATE (object);

    if (priv->filter_mem) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->filter_mem));
        priv->filter_mem = NULL;
    }
    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }
    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }

    G_OBJECT_CLASS (ufo_filter_stripes1d_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_filter_stripes1d_task_setup;
    iface->get_num_inputs = ufo_filter_stripes1d_task_get_num_inputs;
    iface->get_num_dimensions = ufo_filter_stripes1d_task_get_num_dimensions;
    iface->get_mode = ufo_filter_stripes1d_task_get_mode;
    iface->get_requisition = ufo_filter_stripes1d_task_get_requisition;
    iface->process = ufo_filter_stripes1d_task_process;
}

static void
ufo_filter_stripes1d_task_class_init (UfoFilterStripes1dTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_filter_stripes1d_task_set_property;
    oclass->get_property = ufo_filter_stripes1d_task_get_property;
    oclass->finalize = ufo_filter_stripes1d_task_finalize;

    properties[PROP_STRENGTH] =
        g_param_spec_float ("strength",
            "Filter strength",
            "Filter strength, it is the full width at half maximum of a Gaussian "\
            "in the frequency domain. The real filter is then 1 - Gaussian",
            1e-3f, G_MAXFLOAT, 1.0f,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoFilterStripes1dTaskPrivate));
}

static void
ufo_filter_stripes1d_task_init(UfoFilterStripes1dTask *self)
{
    self->priv = UFO_FILTER_STRIPES1D_TASK_GET_PRIVATE(self);

    self->priv->strength = 1.0f;
    self->priv->last_width = 0;
}
