/*
 * Copyright (C) 2011-2016 Karlsruhe Institute of Technology
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

#include "ufo-sleep-task.h"


struct _UfoSleepTaskPrivate {
    gdouble time;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoSleepTask, ufo_sleep_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_SLEEP_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_SLEEP_TASK, UfoSleepTaskPrivate))

enum {
    PROP_0,
    PROP_TIME,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_sleep_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_SLEEP_TASK, NULL));
}

static void
ufo_sleep_task_setup (UfoTask *task,
                      UfoResources *resources,
                      GError **error)
{
}

static void
ufo_sleep_task_get_requisition (UfoTask *task,
                                UfoBuffer **inputs,
                                UfoRequisition *requisition,
                                GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_sleep_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_sleep_task_get_num_dimensions (UfoTask *task,
                                   guint input)
{
    return 2;
}

static UfoTaskMode
ufo_sleep_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_sleep_task_process (UfoTask *task,
                       UfoBuffer **inputs,
                       UfoBuffer *output,
                       UfoRequisition *requisition)
{
    UfoSleepTaskPrivate *priv;

    priv = UFO_SLEEP_TASK_GET_PRIVATE (task);

    g_usleep (priv->time * G_USEC_PER_SEC);
    ufo_buffer_copy (inputs[0], output);
    return TRUE;
}

static void
ufo_sleep_task_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    UfoSleepTaskPrivate *priv = UFO_SLEEP_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_TIME:
            priv->time = g_value_get_double (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_sleep_task_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    UfoSleepTaskPrivate *priv = UFO_SLEEP_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_TIME:
            g_value_set_double (value, priv->time);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_sleep_task_setup;
    iface->get_num_inputs = ufo_sleep_task_get_num_inputs;
    iface->get_num_dimensions = ufo_sleep_task_get_num_dimensions;
    iface->get_mode = ufo_sleep_task_get_mode;
    iface->get_requisition = ufo_sleep_task_get_requisition;
    iface->process = ufo_sleep_task_process;
}

static void
ufo_sleep_task_class_init (UfoSleepTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_sleep_task_set_property;
    gobject_class->get_property = ufo_sleep_task_get_property;

    properties[PROP_TIME] =
        g_param_spec_double ("time",
            "Time to sleep in seconds",
            "Time to sleep in seconds",
            0.0, G_MAXDOUBLE, 1.0,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoSleepTaskPrivate));
}

static void
ufo_sleep_task_init(UfoSleepTask *self)
{
    self->priv = UFO_SLEEP_TASK_GET_PRIVATE(self);
    self->priv->time = 1.0;
}
