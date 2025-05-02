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

#include "ufo-filter-stripes-task.h"


struct _UfoFilterStripesTaskPrivate {
    gfloat horizontal_sigma;
    gfloat vertical_sigma;
    cl_kernel kernel;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoFilterStripesTask, ufo_filter_stripes_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_FILTER_STRIPES_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_STRIPES_TASK, UfoFilterStripesTaskPrivate))

enum {
    PROP_0,
    PROP_HORIZONTAL_SIGMA,
    PROP_VERTICAL_SIGMA,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_filter_stripes_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_FILTER_STRIPES_TASK, NULL));
}

static gboolean
ufo_filter_stripes_task_process (UfoTask *task,
                         UfoBuffer **inputs,
                         UfoBuffer *output,
                         UfoRequisition *requisition)
{
    UfoFilterStripesTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    cl_mem in_mem;
    cl_mem out_mem;

    priv = UFO_FILTER_STRIPES_TASK (task)->priv;
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (cl_float), &priv->horizontal_sigma));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 3, sizeof (cl_float), &priv->vertical_sigma));

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, requisition->dims, NULL);

    return TRUE;
}

static void
ufo_filter_stripes_task_setup (UfoTask *task,
                       UfoResources *resources,
                       GError **error)
{
    UfoFilterStripesTaskPrivate *priv;

    priv = UFO_FILTER_STRIPES_TASK_GET_PRIVATE (task);

    priv->kernel = ufo_resources_get_kernel (resources, "filter.cl", "stripe_filter", NULL, error);

    if (priv->kernel != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
}

static void
ufo_filter_stripes_task_get_requisition (UfoTask *task,
                                         UfoBuffer **inputs,
                                         UfoRequisition *requisition,
                                         GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_filter_stripes_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_filter_stripes_task_get_num_dimensions (UfoTask *task,
                               guint input)
{
    g_return_val_if_fail (input == 0, 0);
    return 2;
}

static UfoTaskMode
ufo_filter_stripes_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static void
ufo_filter_stripes_task_finalize (GObject *object)
{
    UfoFilterStripesTaskPrivate *priv;

    priv = UFO_FILTER_STRIPES_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        clReleaseKernel (priv->kernel);
        priv->kernel = NULL;
    }

    G_OBJECT_CLASS (ufo_filter_stripes_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_filter_stripes_task_setup;
    iface->get_requisition = ufo_filter_stripes_task_get_requisition;
    iface->get_num_inputs = ufo_filter_stripes_task_get_num_inputs;
    iface->get_num_dimensions = ufo_filter_stripes_task_get_num_dimensions;
    iface->get_mode = ufo_filter_stripes_task_get_mode;
    iface->process = ufo_filter_stripes_task_process;
}

static void
ufo_filter_stripes_task_set_property (GObject *object,
                                      guint property_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
    UfoFilterStripesTaskPrivate *priv = UFO_FILTER_STRIPES_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_HORIZONTAL_SIGMA:
            priv->horizontal_sigma = g_value_get_float (value);
            break;
        case PROP_VERTICAL_SIGMA:
            priv->vertical_sigma = g_value_get_float (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_filter_stripes_task_get_property (GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
    UfoFilterStripesTaskPrivate *priv = UFO_FILTER_STRIPES_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_HORIZONTAL_SIGMA:
            g_value_set_float (value, priv->horizontal_sigma);
            break;
        case PROP_VERTICAL_SIGMA:
            g_value_set_float (value, priv->vertical_sigma);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_filter_stripes_task_class_init (UfoFilterStripesTaskClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = ufo_filter_stripes_task_finalize;
    oclass->set_property = ufo_filter_stripes_task_set_property;
    oclass->get_property = ufo_filter_stripes_task_get_property;

    properties[PROP_HORIZONTAL_SIGMA] =
        g_param_spec_float ("horizontal-sigma",
            "Sigma of the gaussian window in the horizontal direction",
            "Sigma of the gaussian window in the horizontal direction",
            0.0, G_MAXFLOAT, 1e-7,
            G_PARAM_READWRITE);

    properties[PROP_VERTICAL_SIGMA] =
        g_param_spec_float ("vertical-sigma",
            "Sigma of the gaussian window in the vertical direction",
            "Sigma of the gaussian window in the vertical direction",
            0.0, G_MAXFLOAT, 0.0f,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private(klass, sizeof(UfoFilterStripesTaskPrivate));
}

static void
ufo_filter_stripes_task_init (UfoFilterStripesTask *self)
{
    UfoFilterStripesTaskPrivate *priv;
    self->priv = priv = UFO_FILTER_STRIPES_TASK_GET_PRIVATE (self);
    priv->kernel = NULL;
    priv->horizontal_sigma = 1e-7;
    priv->vertical_sigma = 0.0f;
}
