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

#include "ufo-detect-edge-task.h"


typedef enum {
    FILTER_SOBEL = 0,
    FILTER_LAPLACE,
    FILTER_PREWITT,
} Filter;

static GEnumValue filter_values[] = {
    { FILTER_SOBEL,     "FILTER_SOBEL",     "sobel" },
    { FILTER_LAPLACE,   "FILTER_LAPLACE",   "laplace" },
    { FILTER_PREWITT,   "FILTER_PREWITT",   "prewitt" },
    { 0, NULL, NULL}
};

struct _UfoDetectEdgeTaskPrivate {
    Filter type;
    cl_context context;
    cl_kernel kernel;
    cl_mem mask_mem;
};

static gfloat FILTER_MASKS[][9] = {
    {  1.0f,  0.0f, -1.0f,   /* Sobel in one direction */
       2.0f,  0.0f, -2.0f,
       1.0f,  0.0f, -1.0f },
    {  0.0f,  1.0f,  0.0f,    /* Laplace */
       1.0f, -4.0f,  1.0f,
       0.0f,  1.0f,  0.0f },
    { -1.0f,  0.0f,  1.0f,    /* Prewitt */
      -1.0f,  0.0f,  1.0f,
      -1.0f,  0.0f,  1.0f }
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoDetectEdgeTask, ufo_detect_edge_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_DETECT_EDGE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_DETECT_EDGE_TASK, UfoDetectEdgeTaskPrivate))

enum {
    PROP_0,
    PROP_TYPE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_detect_edge_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_DETECT_EDGE_TASK, NULL));
}

static void
ufo_detect_edge_task_setup (UfoTask *task,
                            UfoResources *resources,
                            GError **error)
{
    UfoDetectEdgeTaskPrivate *priv;
    cl_int err;

    priv = UFO_DETECT_EDGE_TASK_GET_PRIVATE (task);
    priv->context = ufo_resources_get_context (resources);
    priv->kernel = ufo_resources_get_kernel (resources, "edge.cl", "filter", NULL, error);

    if (priv->mask_mem)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clReleaseMemObject (priv->mask_mem), error);

    priv->mask_mem = clCreateBuffer (priv->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                     9 * sizeof (gfloat), FILTER_MASKS[priv->type], &err);
    UFO_RESOURCES_CHECK_AND_SET (err, error);
}

static void
ufo_detect_edge_task_get_requisition (UfoTask *task,
                                      UfoBuffer **inputs,
                                      UfoRequisition *requisition,
                                      GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_detect_edge_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_detect_edge_task_get_num_dimensions (UfoTask *task,
                                         guint input)
{
    return 2;
}

static UfoTaskMode
ufo_detect_edge_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_detect_edge_task_process (UfoTask *task,
                              UfoBuffer **inputs,
                              UfoBuffer *output,
                              UfoRequisition *requisition)
{
    UfoDetectEdgeTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    cl_mem in_image;
    cl_mem out_mem;
    gchar second_pass;

    priv = UFO_DETECT_EDGE_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    in_image = ufo_buffer_get_device_image (inputs[0], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    second_pass = priv->type == FILTER_SOBEL || priv->type == FILTER_PREWITT;

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &in_image));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &priv->mask_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (cl_char), &second_pass));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 3, sizeof (cl_mem), &out_mem));

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, requisition->dims, NULL);

    return TRUE;
}

static void
ufo_detect_edge_task_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
    UfoDetectEdgeTaskPrivate *priv;
    
    priv = UFO_DETECT_EDGE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_TYPE:
            priv->type = g_value_get_enum (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_detect_edge_task_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
    UfoDetectEdgeTaskPrivate *priv;
    
    priv = UFO_DETECT_EDGE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_TYPE:
            g_value_set_enum (value, priv->type);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_detect_edge_task_finalize (GObject *object)
{
    UfoDetectEdgeTaskPrivate *priv;

    priv = UFO_DETECT_EDGE_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }

    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }

    G_OBJECT_CLASS (ufo_detect_edge_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_detect_edge_task_setup;
    iface->get_num_inputs = ufo_detect_edge_task_get_num_inputs;
    iface->get_num_dimensions = ufo_detect_edge_task_get_num_dimensions;
    iface->get_mode = ufo_detect_edge_task_get_mode;
    iface->get_requisition = ufo_detect_edge_task_get_requisition;
    iface->process = ufo_detect_edge_task_process;
}

static void
ufo_detect_edge_task_class_init (UfoDetectEdgeTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_detect_edge_task_set_property;
    oclass->get_property = ufo_detect_edge_task_get_property;
    oclass->finalize = ufo_detect_edge_task_finalize;

    properties[PROP_TYPE] =
        g_param_spec_enum ("filter",
            "Filter type (\"sobel\", \"laplace\", \"prewitt\")",
            "Filter type (\"sobel\", \"laplace\", \"prewitt\")",
            g_enum_register_static ("ufo_detect_edge_filter", filter_values),
            FILTER_SOBEL,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoDetectEdgeTaskPrivate));
}

static void
ufo_detect_edge_task_init(UfoDetectEdgeTask *self)
{
    self->priv = UFO_DETECT_EDGE_TASK_GET_PRIVATE(self);
    self->priv->type = FILTER_SOBEL;
    self->priv->context = NULL;
    self->priv->kernel = NULL;
    self->priv->mask_mem = NULL;
}
