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

#include <stdio.h>
#include "ufo-null-task.h"


static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoNullTask, ufo_null_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_NULL_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_NULL_TASK, UfoNullTaskPrivate))

struct _UfoNullTaskPrivate {
    gboolean download;
    gboolean finish;
    gboolean durations;
};

enum {
    PROP_0,
    PROP_DOWNLOAD,
    PROP_FINISH,
    PROP_DURATIONS,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_null_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_NULL_TASK, NULL));
}

static void
ufo_null_task_setup (UfoTask *task,
                     UfoResources *resources,
                     GError **error)
{
}

static void
ufo_null_task_get_requisition (UfoTask *task,
                               UfoBuffer **inputs,
                               UfoRequisition *requisition,
                               GError **error)
{
    requisition->n_dims = 0;
}

static guint
ufo_null_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_null_task_get_num_dimensions (UfoTask *task,
                                  guint input)
{
    g_return_val_if_fail (input == 0, 0);
    return 2;
}

static UfoTaskMode
ufo_null_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_SINK | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_null_task_process (UfoTask *task,
                       UfoBuffer **inputs,
                       UfoBuffer *output,
                       UfoRequisition *requisition)
{
    UfoNullTaskPrivate *priv;

    priv = UFO_NULL_TASK_GET_PRIVATE (task);

    if (priv->durations) {
        GValue *timestamp;

        timestamp = ufo_buffer_get_metadata (inputs[0], "ts");

        if (timestamp != NULL) {
            gint64 start;
            gint64 now;

            start = g_value_get_int64 (timestamp);
            now = g_get_real_time ();
            fprintf (stderr, "%f\n", (now - start) / 1000.0f);
        }
    }

    if (priv->download) {
        gfloat *host_array;

        host_array = ufo_buffer_get_host_array (inputs[0], NULL);
        host_array[0] = 0.0;
    }

    if (priv->finish) {
        UfoGpuNode *gpu;

        gpu = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
        UFO_RESOURCES_CHECK_CLERR (clFinish (ufo_gpu_node_get_cmd_queue (gpu)));
    }

    return TRUE;
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_null_task_setup;
    iface->get_num_inputs = ufo_null_task_get_num_inputs;
    iface->get_num_dimensions = ufo_null_task_get_num_dimensions;
    iface->get_mode = ufo_null_task_get_mode;
    iface->get_requisition = ufo_null_task_get_requisition;
    iface->process = ufo_null_task_process;
}

static void
ufo_null_task_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    UfoNullTaskPrivate *priv = UFO_NULL_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_DOWNLOAD:
            priv->download = g_value_get_boolean (value);
            break;

        case PROP_FINISH:
            priv->finish = g_value_get_boolean (value);
            break;

        case PROP_DURATIONS:
            priv->durations = g_value_get_boolean (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_null_task_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    UfoNullTaskPrivate *priv = UFO_NULL_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_DOWNLOAD:
            g_value_set_boolean (value, priv->download);
            break;

        case PROP_FINISH:
            g_value_set_boolean (value, priv->finish);
            break;

        case PROP_DURATIONS:
            g_value_set_boolean (value, priv->durations);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_null_task_class_init (UfoNullTaskClass *klass)
{
    GObjectClass *oclass;
    oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_null_task_set_property;
    oclass->get_property = ufo_null_task_get_property;

    properties[PROP_DOWNLOAD] =
        g_param_spec_boolean ("download",
            "Force data to be transferred from device to host",
            "Force data to be transferred from device to host",
            FALSE, G_PARAM_READWRITE);

    properties[PROP_FINISH] =
        g_param_spec_boolean ("finish",
            "Call finish on the associated command queue",
            "Call finish on the associated command queue",
            FALSE, G_PARAM_READWRITE);

    properties[PROP_DURATIONS] =
        g_param_spec_boolean ("durations",
            "Show durations for timestamps",
            "Show durations for timestamps",
            FALSE, G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private(klass, sizeof (UfoNullTaskPrivate));
}

static void
ufo_null_task_init(UfoNullTask *self)
{
    UfoNullTaskPrivate *priv;

    self->priv = priv = UFO_NULL_TASK_GET_PRIVATE (self);
    priv->download = FALSE;
    priv->finish = FALSE;
    priv->durations = FALSE;
}
