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
#include "ufo-interpolate-stream-task.h"


struct _UfoInterpolateStreamTaskPrivate {
    guint num_inputs;
    guint number;
    guint current;
    GPtrArray *copies;
    cl_kernel kernel;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoInterpolateStreamTask, ufo_interpolate_stream_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_INTERPOLATE_STREAM_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_INTERPOLATE_STREAM_TASK, UfoInterpolateStreamTaskPrivate))

enum {
    PROP_0,
    PROP_NUMBER,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_interpolate_stream_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_INTERPOLATE_STREAM_TASK, NULL));
}

static void
ufo_interpolate_stream_task_setup (UfoTask *task,
                                   UfoResources *resources,
                                   GError **error)
{
    UfoInterpolateStreamTaskPrivate *priv;

    priv = UFO_INTERPOLATE_STREAM_TASK_GET_PRIVATE (task);
    priv->num_inputs = 0;
    priv->kernel = ufo_resources_get_kernel (resources, "interpolator.cl", "interpolate", NULL, error);

    if (priv->kernel != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
}

static void
ufo_interpolate_stream_task_get_requisition (UfoTask *task,
                                             UfoBuffer **inputs,
                                             UfoRequisition *requisition,
                                             GError **error)
{
    /* TODO: check it's the same all the time */
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_interpolate_stream_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_interpolate_stream_task_get_num_dimensions (UfoTask *task,
                                                guint input)
{
    return 2;
}

static UfoTaskMode
ufo_interpolate_stream_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_REDUCTOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_interpolate_stream_task_process (UfoTask *task,
                                     UfoBuffer **inputs,
                                     UfoBuffer *output,
                                     UfoRequisition *requisition)
{
    UfoInterpolateStreamTaskPrivate *priv;
    UfoBuffer *copy;

    priv = UFO_INTERPOLATE_STREAM_TASK_GET_PRIVATE (task);
    priv->num_inputs++;

    copy = ufo_buffer_dup (inputs[0]);
    ufo_buffer_copy (inputs[0], copy);
    g_ptr_array_add (priv->copies, copy);

    return TRUE;
}

static gboolean
ufo_interpolate_stream_task_generate (UfoTask *task,
                                      UfoBuffer *output,
                                      UfoRequisition *requisition)
{
    UfoInterpolateStreamTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    UfoBuffer *buffer_x;
    UfoBuffer *buffer_y;
    cl_command_queue cmd_queue;
    cl_mem x_mem;
    cl_mem y_mem;
    cl_mem out_mem;
    gfloat alpha;
    guint lower;
    guint num_interpolate;
    guint current;

    priv = UFO_INTERPOLATE_STREAM_TASK_GET_PRIVATE (task);

    if (priv->current == priv->number)
        return FALSE;

    lower = (guint) floor ((priv->num_inputs - 1) * ((gfloat) priv->current) / priv->number);
    num_interpolate = (guint) roundf (priv->number / ((gfloat) priv->num_inputs - 1));

    /* index in the current segment */
    current = priv->current - num_interpolate * lower;

    /* adjust for last segment */
    num_interpolate = MIN ((lower + 1) * num_interpolate, priv->number) - lower * num_interpolate;
    alpha = current / ((gfloat) num_interpolate);

    g_assert (lower + 1 < priv->copies->len);
    buffer_x = g_ptr_array_index (priv->copies, lower);
    buffer_y = g_ptr_array_index (priv->copies, lower + 1);

    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    x_mem = ufo_buffer_get_device_array (buffer_x, cmd_queue);
    y_mem = ufo_buffer_get_device_array (buffer_y, cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &x_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &y_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 3, sizeof (gfloat), &alpha));

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, requisition->dims, NULL);
    priv->current++;
    return TRUE;
}

static void
ufo_interpolate_stream_task_set_property (GObject *object,
                                          guint property_id,
                                          const GValue *value,
                                          GParamSpec *pspec)
{
    UfoInterpolateStreamTaskPrivate *priv = UFO_INTERPOLATE_STREAM_TASK_GET_PRIVATE (object);

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
ufo_interpolate_stream_task_get_property (GObject *object,
                                          guint property_id,
                                          GValue *value,
                                          GParamSpec *pspec)
{
    UfoInterpolateStreamTaskPrivate *priv = UFO_INTERPOLATE_STREAM_TASK_GET_PRIVATE (object);

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
ufo_interpolate_stream_task_dispose (GObject *object)
{
    UfoInterpolateStreamTaskPrivate *priv;

    priv = UFO_INTERPOLATE_STREAM_TASK_GET_PRIVATE (object);

    for (guint i = 0; i < priv->copies->len; i++)
        g_object_unref (g_ptr_array_index (priv->copies, i));

    G_OBJECT_CLASS (ufo_interpolate_stream_task_parent_class)->dispose (object);
}

static void
ufo_interpolate_stream_task_finalize (GObject *object)
{
    UfoInterpolateStreamTaskPrivate *priv;

    priv = UFO_INTERPOLATE_STREAM_TASK_GET_PRIVATE (object);
    g_ptr_array_free (priv->copies, TRUE);

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }

    G_OBJECT_CLASS (ufo_interpolate_stream_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_interpolate_stream_task_setup;
    iface->get_num_inputs = ufo_interpolate_stream_task_get_num_inputs;
    iface->get_num_dimensions = ufo_interpolate_stream_task_get_num_dimensions;
    iface->get_mode = ufo_interpolate_stream_task_get_mode;
    iface->get_requisition = ufo_interpolate_stream_task_get_requisition;
    iface->process = ufo_interpolate_stream_task_process;
    iface->generate = ufo_interpolate_stream_task_generate;
}

static void
ufo_interpolate_stream_task_class_init (UfoInterpolateStreamTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_interpolate_stream_task_set_property;
    oclass->get_property = ufo_interpolate_stream_task_get_property;
    oclass->dispose = ufo_interpolate_stream_task_dispose;
    oclass->finalize = ufo_interpolate_stream_task_finalize;

    properties[PROP_NUMBER] =
        g_param_spec_uint ("number",
            "Number of interpolated images",
            "Number of interpolated images",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoInterpolateStreamTaskPrivate));
}

static void
ufo_interpolate_stream_task_init(UfoInterpolateStreamTask *self)
{
    self->priv = UFO_INTERPOLATE_STREAM_TASK_GET_PRIVATE(self);
    self->priv->number = 1;
    self->priv->copies = g_ptr_array_new ();
}
