/*
 * Copyright (C) 2011-2014 Karlsruhe Institute of Technology
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

#include <string.h>
#include "ufo-stack-task.h"


struct _UfoStackTaskPrivate {
    guint n_items;
    guint current;
    gboolean inputs_stopped;
    gboolean finished;
    gboolean generated;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoStackTask, ufo_stack_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_STACK_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_STACK_TASK, UfoStackTaskPrivate))

enum {
    PROP_0,
    PROP_NUM_ITEMS,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_stack_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_STACK_TASK, NULL));
}

static void
ufo_stack_task_setup (UfoTask *task,
                      UfoResources *resources,
                      GError **error)
{
    UfoStackTaskPrivate *priv;

    priv = UFO_STACK_TASK_GET_PRIVATE (task);
    priv->current = 0;
    priv->generated = FALSE;
}

static void
ufo_stack_task_get_requisition (UfoTask *task,
                                UfoBuffer **inputs,
                                UfoRequisition *requisition,
                                GError **error)
{
    UfoStackTaskPrivate *priv;

    priv = UFO_STACK_TASK_GET_PRIVATE (task);

    ufo_buffer_get_requisition (inputs[0], requisition);

    requisition->n_dims = 3;
    requisition->dims[2] = priv->n_items;
}

static guint
ufo_stack_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_stack_task_get_num_dimensions (UfoTask *task,
                                   guint input)
{
    return 2;
}

static UfoTaskMode
ufo_stack_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_REDUCTOR | UFO_TASK_MODE_CPU;
}

static gboolean
ufo_stack_task_process (UfoTask *task,
                        UfoBuffer **inputs,
                        UfoBuffer *output,
                        UfoRequisition *requisition)
{
    UfoStackTaskPrivate *priv;
    guint8 *in_mem;
    guint8 *out_mem;
    gsize size;

    priv = UFO_STACK_TASK_GET_PRIVATE (task);

    size = ufo_buffer_get_size (inputs[0]);
    in_mem = (guint8 *) ufo_buffer_get_host_array (inputs[0], NULL);
    out_mem = (guint8 *) ufo_buffer_get_host_array (output, NULL);
    memcpy (out_mem + (priv->current % priv->n_items) * size, in_mem, size);
    priv->current++;

    if (priv->current % priv->n_items == 0) {
        // g_warning ("StackTask: stack full, ready for generating. [current %d]", priv->current);
        priv->generated = FALSE;
        return FALSE;
    }

    return TRUE;
}

static gboolean
ufo_stack_task_generate (UfoTask *task,
                         UfoBuffer *output,
                         UfoRequisition *requisition)
{
    UfoStackTaskPrivate *priv;

    priv = UFO_STACK_TASK_GET_PRIVATE (task);

    if (priv->inputs_stopped && !priv->finished) {
        /* If the inputs stopped and n_items is not a divisor of the length of
         * the input stream, force generation to make sure the last chunk of
         * data is produced, even though with invalid last elements (leftovers
         * from previous stack filling) */
        priv->generated = FALSE;
        priv->finished = TRUE;
    }

    if (!priv->generated) {
        priv->generated = TRUE;
        return TRUE;
    }
    else {
        return FALSE;
    }
}

static void inputs_stopped_callback (UfoTask *task)
{
    UfoStackTaskPrivate *priv = UFO_STACK_TASK_GET_PRIVATE (task);

    priv->inputs_stopped = TRUE;
}

static void
ufo_stack_task_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    UfoStackTaskPrivate *priv = UFO_STACK_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_ITEMS:
            priv->n_items = g_value_get_uint (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_stack_task_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    UfoStackTaskPrivate *priv = UFO_STACK_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_ITEMS:
            g_value_set_uint (value, priv->n_items);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_stack_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_stack_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_stack_task_setup;
    iface->get_num_inputs = ufo_stack_task_get_num_inputs;
    iface->get_num_dimensions = ufo_stack_task_get_num_dimensions;
    iface->get_mode = ufo_stack_task_get_mode;
    iface->get_requisition = ufo_stack_task_get_requisition;
    iface->process = ufo_stack_task_process;
    iface->generate = ufo_stack_task_generate;
}

static void
ufo_stack_task_class_init (UfoStackTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_stack_task_set_property;
    oclass->get_property = ufo_stack_task_get_property;
    oclass->finalize = ufo_stack_task_finalize;

    properties[PROP_NUM_ITEMS] =
        g_param_spec_uint ("number",
            "Number of expected items",
            "Number of expected items",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoStackTaskPrivate));
}

static void
ufo_stack_task_init(UfoStackTask *self)
{
    self->priv = UFO_STACK_TASK_GET_PRIVATE(self);
    self->priv->n_items = 1;
    self->priv->inputs_stopped = FALSE;
    self->priv->finished = FALSE;
    g_signal_connect (self, "inputs_stopped", (GCallback) inputs_stopped_callback, NULL);
}
