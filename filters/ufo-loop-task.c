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

#include "ufo-loop-task.h"


struct _UfoLoopTaskPrivate {
    guint number;
    guint current;
    UfoBuffer *temporary;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoLoopTask, ufo_loop_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_LOOP_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_LOOP_TASK, UfoLoopTaskPrivate))

enum {
    PROP_0,
    PROP_NUMBER,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_loop_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_LOOP_TASK, NULL));
}

static void
ufo_loop_task_setup (UfoTask *task,
                     UfoResources *resources,
                     GError **error)
{
}

static void
ufo_loop_task_get_requisition (UfoTask *task,
                               UfoBuffer **inputs,
                               UfoRequisition *requisition,
                               GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_loop_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_loop_task_get_num_dimensions (UfoTask *task,
                                  guint input)
{
    return 2;
}

static UfoTaskMode
ufo_loop_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_REDUCTOR | UFO_TASK_MODE_CPU;
}

static gboolean
ufo_loop_task_process (UfoTask *task,
                       UfoBuffer **inputs,
                       UfoBuffer *output,
                       UfoRequisition *requisition)
{
    UfoLoopTaskPrivate *priv;

    priv = UFO_LOOP_TASK_GET_PRIVATE (task);

    if (priv->temporary == NULL)
        priv->temporary = ufo_buffer_dup (inputs[0]);

    ufo_buffer_copy (inputs[0], priv->temporary);
    priv->current = 0;

    /*
     * We processed the item but have to return FALSE to trigger the generate
     * phase, otherwise we will receive a new item which we do not store in a
     * temporary buffer.
     */
    return FALSE;
}

static gboolean
ufo_loop_task_generate (UfoTask *task,
                        UfoBuffer *output,
                        UfoRequisition *requisition)
{
    UfoLoopTaskPrivate *priv;

    priv = UFO_LOOP_TASK_GET_PRIVATE (task);

    if (priv->current == priv->number)
        return FALSE;

    ufo_buffer_copy (priv->temporary, output);
    priv->current++;
    return TRUE;
}

static void
ufo_loop_task_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    UfoLoopTaskPrivate *priv = UFO_LOOP_TASK_GET_PRIVATE (object);

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
ufo_loop_task_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    UfoLoopTaskPrivate *priv = UFO_LOOP_TASK_GET_PRIVATE (object);

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
ufo_loop_task_dispose (GObject *object)
{
    UfoLoopTaskPrivate *priv;

    priv = UFO_LOOP_TASK_GET_PRIVATE (object);
    if (priv->temporary) {
        g_object_unref (priv->temporary);
        priv->temporary = NULL;
    }

    G_OBJECT_CLASS (ufo_loop_task_parent_class)->dispose (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_loop_task_setup;
    iface->get_num_inputs = ufo_loop_task_get_num_inputs;
    iface->get_num_dimensions = ufo_loop_task_get_num_dimensions;
    iface->get_mode = ufo_loop_task_get_mode;
    iface->get_requisition = ufo_loop_task_get_requisition;
    iface->process = ufo_loop_task_process;
    iface->generate = ufo_loop_task_generate;
}

static void
ufo_loop_task_class_init (UfoLoopTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_loop_task_set_property;
    oclass->get_property = ufo_loop_task_get_property;
    oclass->dispose = ufo_loop_task_dispose;

    properties[PROP_NUMBER] =
        g_param_spec_uint ("number",
            "Number of loop iterations",
            "Number of loop iterations",
            0, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoLoopTaskPrivate));
}

static void
ufo_loop_task_init(UfoLoopTask *self)
{
    self->priv = UFO_LOOP_TASK_GET_PRIVATE(self);
    self->priv->number = 1;
    self->priv->temporary = NULL;
}
