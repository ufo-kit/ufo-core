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

#include "ufo-flip-task.h"

typedef enum {
    DIRECTION_HORIZONTAL = 0,
    DIRECTION_VERTICAL
} Direction;

static GEnumValue direction_values[] = {
    { DIRECTION_HORIZONTAL, "DIRECTION_HORIZONTAL", "horizontal" },
    { DIRECTION_VERTICAL,   "DIRECTION_VERTICAL",   "vertical" },
    { 0, NULL, NULL}
};

struct _UfoFlipTaskPrivate {
    Direction direction;
    cl_kernel kernels[2];
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoFlipTask, ufo_flip_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_FLIP_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FLIP_TASK, UfoFlipTaskPrivate))

enum {
    PROP_0,
    PROP_DIRECTION,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_flip_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_FLIP_TASK, NULL));
}

static void
ufo_flip_task_setup (UfoTask *task,
                     UfoResources *resources,
                     GError **error)
{
    UfoFlipTaskPrivate *priv;

    priv = UFO_FLIP_TASK_GET_PRIVATE (task);
    priv->kernels[DIRECTION_HORIZONTAL] = ufo_resources_get_kernel (resources, "flip.cl", "flip_horizontal", NULL, error);
    priv->kernels[DIRECTION_VERTICAL] = ufo_resources_get_kernel (resources, "flip.cl", "flip_vertical", NULL, error);
}

static void
ufo_flip_task_get_requisition (UfoTask *task,
                               UfoBuffer **inputs,
                               UfoRequisition *requisition,
                               GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_flip_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_flip_task_get_num_dimensions (UfoTask *task,
                                  guint input)
{
    return 2;
}

static UfoTaskMode
ufo_flip_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_flip_task_process (UfoTask *task,
                       UfoBuffer **inputs,
                       UfoBuffer *output,
                       UfoRequisition *requisition)
{
    UfoFlipTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    cl_mem in_mem;
    cl_mem out_mem;
    cl_kernel kernel;

    priv = UFO_FLIP_TASK_GET_PRIVATE (task);
    kernel = priv->kernels[priv->direction];
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 1, sizeof (cl_mem), &out_mem));

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, kernel, 2, requisition->dims, NULL);

    return TRUE;
}

static void
ufo_flip_task_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    UfoFlipTaskPrivate *priv = UFO_FLIP_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_DIRECTION:
            priv->direction = g_value_get_enum (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_flip_task_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    UfoFlipTaskPrivate *priv = UFO_FLIP_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_DIRECTION:
            g_value_set_enum (value, priv->direction);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_flip_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_flip_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_flip_task_setup;
    iface->get_num_inputs = ufo_flip_task_get_num_inputs;
    iface->get_num_dimensions = ufo_flip_task_get_num_dimensions;
    iface->get_mode = ufo_flip_task_get_mode;
    iface->get_requisition = ufo_flip_task_get_requisition;
    iface->process = ufo_flip_task_process;
}

static void
ufo_flip_task_class_init (UfoFlipTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_flip_task_set_property;
    oclass->get_property = ufo_flip_task_get_property;
    oclass->finalize = ufo_flip_task_finalize;

    properties[PROP_DIRECTION] =
        g_param_spec_enum ("direction",
            "Flip direction (`horizontal' or `vertical')",
            "Flip direction (`horizontal' or `vertical')",
            g_enum_register_static ("ufo_flip_direction", direction_values),
            DIRECTION_HORIZONTAL,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoFlipTaskPrivate));
}

static void
ufo_flip_task_init(UfoFlipTask *self)
{
    self->priv = UFO_FLIP_TASK_GET_PRIVATE(self);
    self->priv->direction = DIRECTION_HORIZONTAL;
    self->priv->kernels[DIRECTION_HORIZONTAL] = NULL;
    self->priv->kernels[DIRECTION_VERTICAL] = NULL;
}
