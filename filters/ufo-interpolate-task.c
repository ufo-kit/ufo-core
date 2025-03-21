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

#include "ufo-interpolate-task.h"


struct _UfoInterpolateTaskPrivate {
    cl_mem x;
    cl_mem y;
    cl_context context;
    cl_kernel kernel;
    guint number;
    guint current;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoInterpolateTask, ufo_interpolate_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_INTERPOLATE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_INTERPOLATE_TASK, UfoInterpolateTaskPrivate))

enum {
    PROP_0,
    PROP_NUMBER,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_interpolate_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_INTERPOLATE_TASK, NULL));
}

static void
ufo_interpolate_task_setup (UfoTask *task,
                            UfoResources *resources,
                            GError **error)
{
    UfoInterpolateTaskPrivate *priv;

    priv = UFO_INTERPOLATE_TASK_GET_PRIVATE (task);
    priv->current = 0;
    priv->context = ufo_resources_get_context (resources);
    priv->kernel = ufo_resources_get_kernel (resources, "interpolator.cl", "interpolate", NULL, error);

    if (priv->kernel != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
}

static void
ufo_interpolate_task_get_requisition (UfoTask *task,
                                      UfoBuffer **inputs,
                                      UfoRequisition *requisition,
                                      GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);

    if (ufo_buffer_cmp_dimensions (inputs[1], requisition) != 0) {
        g_set_error_literal (error, UFO_TASK_ERROR, UFO_TASK_ERROR_GET_REQUISITION,
                             "interpolate inputs must have the same size");
    }
}

static guint
ufo_interpolate_task_get_num_inputs (UfoTask *task)
{
    return 2;
}

static guint
ufo_interpolate_task_get_num_dimensions (UfoTask *task,
                                         guint input)
{
    return 2;
}

static UfoTaskMode
ufo_interpolate_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_REDUCTOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_interpolate_task_process (UfoTask *task,
                              UfoBuffer **inputs,
                              UfoBuffer *output,
                              UfoRequisition *requisition)
{
    UfoInterpolateTaskPrivate *priv;
    cl_int error;

    priv = UFO_INTERPOLATE_TASK_GET_PRIVATE (task);

    if (priv->x != NULL || priv->y != NULL)
        return FALSE;

    if (priv->x == NULL && priv->y == NULL) {
        priv->x = clCreateBuffer (priv->context, CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY,
                                  ufo_buffer_get_size (inputs[0]),
                                  ufo_buffer_get_host_array (inputs[0], NULL), &error);
        UFO_RESOURCES_CHECK_CLERR (error);

        priv->y = clCreateBuffer (priv->context, CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY,
                                  ufo_buffer_get_size (inputs[1]),
                                  ufo_buffer_get_host_array (inputs[1], NULL), &error);
        UFO_RESOURCES_CHECK_CLERR (error);

        return TRUE;
    }

    return FALSE;
}

static gboolean
ufo_interpolate_task_generate (UfoTask *task,
                               UfoBuffer *output,
                               UfoRequisition *requisition)
{
    UfoInterpolateTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    cl_mem out_mem;
    gfloat alpha;

    priv = UFO_INTERPOLATE_TASK_GET_PRIVATE (task);

    if (priv->current == priv->number)
        return FALSE;

    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    alpha = priv->number > 1 ? ((gfloat) priv->current) / (priv->number - 1) : 0;

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &priv->x));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &priv->y));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 3, sizeof (gfloat), &alpha));

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, requisition->dims, NULL);
    priv->current++;

    return TRUE;
}

static void
ufo_interpolate_task_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
    UfoInterpolateTaskPrivate *priv = UFO_INTERPOLATE_TASK_GET_PRIVATE (object);

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
ufo_interpolate_task_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
    UfoInterpolateTaskPrivate *priv = UFO_INTERPOLATE_TASK_GET_PRIVATE (object);

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
ufo_interpolate_task_finalize (GObject *object)
{
    UfoInterpolateTaskPrivate *priv;

    priv = UFO_INTERPOLATE_TASK_GET_PRIVATE (UFO_INTERPOLATE_TASK (object));

    if (priv->x != NULL) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->x));
        priv->x = NULL;
    }

    if (priv->y != NULL) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->y));
        priv->y = NULL;
    }

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }

    G_OBJECT_CLASS (ufo_interpolate_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_interpolate_task_setup;
    iface->get_num_inputs = ufo_interpolate_task_get_num_inputs;
    iface->get_num_dimensions = ufo_interpolate_task_get_num_dimensions;
    iface->get_mode = ufo_interpolate_task_get_mode;
    iface->get_requisition = ufo_interpolate_task_get_requisition;
    iface->process = ufo_interpolate_task_process;
    iface->generate = ufo_interpolate_task_generate;
}

static void
ufo_interpolate_task_class_init (UfoInterpolateTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_interpolate_task_set_property;
    oclass->get_property = ufo_interpolate_task_get_property;
    oclass->finalize = ufo_interpolate_task_finalize;

    properties[PROP_NUMBER] =
        g_param_spec_uint ("number",
            "Number of interpolated images",
            "Number of interpolated images",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoInterpolateTaskPrivate));
}

static void
ufo_interpolate_task_init(UfoInterpolateTask *self)
{
    self->priv = UFO_INTERPOLATE_TASK_GET_PRIVATE(self);
    self->priv->x = NULL;
    self->priv->y = NULL;
    self->priv->number = 1;
}
