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

#include "ufo-gradient-task.h"

typedef enum {
    DIRECTION_HORIZONTAL,
    DIRECTION_VERTICAL,
    DIRECTION_BOTH,
    DIRECTION_BOTH_ABS,
    DIRECTION_LAST
} Direction;

static GEnumValue direction_values[] = {
    {DIRECTION_HORIZONTAL,      "DIRECTION_HORIZONTAL",     "horizontal" },
    {DIRECTION_VERTICAL,        "DIRECTION_VERTICAL",       "vertical"   },
    {DIRECTION_BOTH,            "DIRECTION_BOTH",           "both"       },
    {DIRECTION_BOTH_ABS,        "DIRECTION_BOTH_ABS",       "both_abs"   },
    {0, NULL, NULL}
};

struct _UfoGradientTaskPrivate {
    Direction direction;
    cl_kernel kernel;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoGradientTask, ufo_gradient_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_GRADIENT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GRADIENT_TASK, UfoGradientTaskPrivate))

enum {
    PROP_0,
    PROP_DIRECTION,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_gradient_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_GRADIENT_TASK, NULL));
}

static void
ufo_gradient_task_setup (UfoTask *task,
                         UfoResources *resources,
                         GError **error)
{
    UfoGradientTaskPrivate *priv = UFO_GRADIENT_TASK_GET_PRIVATE (task);
    priv->kernel = ufo_resources_get_kernel (resources, "gradient.cl",
                                             direction_values[priv->direction].value_nick,
                                             NULL, error);
    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
}

static void
ufo_gradient_task_get_requisition (UfoTask *task,
                                   UfoBuffer **inputs,
                                   UfoRequisition *requisition,
                                   GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_gradient_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_gradient_task_get_num_dimensions (UfoTask *task,
                                      guint input)
{
    return 2;
}

static UfoTaskMode
ufo_gradient_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_gradient_task_process (UfoTask *task,
                           UfoBuffer **inputs,
                           UfoBuffer *output,
                           UfoRequisition *requisition)
{
    UfoGradientTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    cl_mem in_mem, out_mem;

    priv = UFO_GRADIENT_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));

    switch (priv->direction) {
        case DIRECTION_BOTH:
        case DIRECTION_BOTH_ABS:
        case DIRECTION_HORIZONTAL:
            if (requisition->dims[0] < 3) {
                g_warning ("Skipping image with width less than 3");
                return TRUE;
            }
            break;
        case DIRECTION_VERTICAL:
            if (requisition->dims[1] < 3) {
                g_warning ("Skipping image with height less than 3");
                return TRUE;
            }
        default:
            break;
    }

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &out_mem));
    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, requisition->dims, NULL);

    return TRUE;
}


static void
ufo_gradient_task_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    UfoGradientTaskPrivate *priv = UFO_GRADIENT_TASK_GET_PRIVATE (object);

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
ufo_gradient_task_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
    UfoGradientTaskPrivate *priv = UFO_GRADIENT_TASK_GET_PRIVATE (object);

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
ufo_gradient_task_finalize (GObject *object)
{
    UfoGradientTaskPrivate *priv = UFO_GRADIENT_TASK_GET_PRIVATE (object);
    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }

    G_OBJECT_CLASS (ufo_gradient_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_gradient_task_setup;
    iface->get_num_inputs = ufo_gradient_task_get_num_inputs;
    iface->get_num_dimensions = ufo_gradient_task_get_num_dimensions;
    iface->get_mode = ufo_gradient_task_get_mode;
    iface->get_requisition = ufo_gradient_task_get_requisition;
    iface->process = ufo_gradient_task_process;
}

static void
ufo_gradient_task_class_init (UfoGradientTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_gradient_task_set_property;
    oclass->get_property = ufo_gradient_task_get_property;
    oclass->finalize = ufo_gradient_task_finalize;

    properties[PROP_DIRECTION] =
        g_param_spec_enum ("direction",
            "Direction (horizontal, vertical, both, both_abs)",
            "Direction (horizontal, vertical, both, both_abs)",
            g_enum_register_static ("ufo_gradient_direction", direction_values),
            DIRECTION_HORIZONTAL,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoGradientTaskPrivate));
}

static void
ufo_gradient_task_init(UfoGradientTask *self)
{
    self->priv = UFO_GRADIENT_TASK_GET_PRIVATE(self);
    self->priv->direction = DIRECTION_HORIZONTAL;
}
