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

#include <string.h>
#include "ufo-sliding-stack-task.h"


struct _UfoSlidingStackTaskPrivate {
    guint n_items;
    gboolean ordered;
    guint current;
    guint8 *window;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoSlidingStackTask, ufo_sliding_stack_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_SLIDING_STACK_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_SLIDING_STACK_TASK, UfoSlidingStackTaskPrivate))

enum {
    PROP_0,
    PROP_NUM_ITEMS,
    PROP_ORDERED,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_sliding_stack_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_SLIDING_STACK_TASK, NULL));
}

static void
ufo_sliding_stack_task_setup (UfoTask *task,
                              UfoResources *resources,
                              GError **error)
{
    UfoSlidingStackTaskPrivate *priv;

    priv = UFO_SLIDING_STACK_TASK_GET_PRIVATE (task);
    priv->current = 0;
}

static void
ufo_sliding_stack_task_get_requisition (UfoTask *task,
                                        UfoBuffer **inputs,
                                        UfoRequisition *requisition,
                                        GError **error)
{
    UfoSlidingStackTaskPrivate *priv;

    priv = UFO_SLIDING_STACK_TASK_GET_PRIVATE (task);

    ufo_buffer_get_requisition (inputs[0], requisition);
    requisition->n_dims = 3;
    requisition->dims[2] = priv->n_items;

    if (priv->window == NULL) {
        priv->window = g_malloc0 (requisition->dims[0] *
                                  requisition->dims[1] *
                                  requisition->dims[2] * sizeof (gfloat));
    }
}

static guint
ufo_sliding_stack_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_sliding_stack_task_get_num_dimensions (UfoTask *task,
                                           guint input)
{
    return 2;
}

static UfoTaskMode
ufo_sliding_stack_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_CPU;
}

static gboolean
ufo_sliding_stack_task_process (UfoTask *task,
                                UfoBuffer **inputs,
                                UfoBuffer *output,
                                UfoRequisition *requisition)
{
    UfoSlidingStackTaskPrivate *priv;
    guint8 *in_mem;
    guint8 *out_mem;
    gsize size, window_size;
    guint i, j;

    priv = UFO_SLIDING_STACK_TASK_GET_PRIVATE (task);

    size = ufo_buffer_get_size (inputs[0]);
    window_size = requisition->dims[0] * requisition->dims[1] * requisition->dims[2] * sizeof (gfloat);
    in_mem = (guint8 *) ufo_buffer_get_host_array (inputs[0], NULL);
    out_mem = (guint8 *) ufo_buffer_get_host_array (output, NULL);
    memcpy (priv->window + priv->current % priv->n_items * size, in_mem, size);
    priv->current++;

    if (priv->current == 1) {
        /* Copy the first input to the whole window. */
        for (i = 1; i < priv->n_items; i++) {
            memcpy (priv->window + i * size, in_mem, size);
        }
    }

    /* The double buffering prevents us to use *out_mem* directly, because we would
     * replace even indices in one output buffer and odd in the other. So we
     * need to keep a copy of the input locally and copy to the output.
     * TODO: improve this on a lower level in ufo-core. */
    if (priv->ordered) {
        j = priv->current;
        for (i = 0; i < priv->n_items; i++, j++) {
            memcpy (out_mem + i * size, priv->window + j % priv->n_items * size, size);
        }
    } else {
        memcpy (out_mem, priv->window, window_size);
    }

    return TRUE;
}


static void
ufo_sliding_stack_task_set_property (GObject *object,
                                     guint property_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
    UfoSlidingStackTaskPrivate *priv = UFO_SLIDING_STACK_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_ITEMS:
            priv->n_items = g_value_get_uint (value);
            break;
        case PROP_ORDERED:
            priv->ordered = g_value_get_boolean (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_sliding_stack_task_get_property (GObject *object,
                                     guint property_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
    UfoSlidingStackTaskPrivate *priv = UFO_SLIDING_STACK_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_ITEMS:
            g_value_set_uint (value, priv->n_items);
            break;
        case PROP_ORDERED:
            g_value_set_boolean (value, priv->ordered);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_sliding_stack_task_finalize (GObject *object)
{
    UfoSlidingStackTaskPrivate *priv;

    priv = UFO_SLIDING_STACK_TASK_GET_PRIVATE (object);

    if (priv->window) {
        g_free (priv->window);
        priv->window = NULL;
    }

    G_OBJECT_CLASS (ufo_sliding_stack_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_sliding_stack_task_setup;
    iface->get_num_inputs = ufo_sliding_stack_task_get_num_inputs;
    iface->get_num_dimensions = ufo_sliding_stack_task_get_num_dimensions;
    iface->get_mode = ufo_sliding_stack_task_get_mode;
    iface->get_requisition = ufo_sliding_stack_task_get_requisition;
    iface->process = ufo_sliding_stack_task_process;
}

static void
ufo_sliding_stack_task_class_init (UfoSlidingStackTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_sliding_stack_task_set_property;
    oclass->get_property = ufo_sliding_stack_task_get_property;
    oclass->finalize = ufo_sliding_stack_task_finalize;

    properties[PROP_NUM_ITEMS] =
        g_param_spec_uint ("number",
            "Number of items in the sliding window",
            "Number of items in the sliding window",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    properties[PROP_ORDERED] =
        g_param_spec_boolean ("ordered",
            "Order items in the sliding window",
            "Order items in the sliding window",
            FALSE, G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoSlidingStackTaskPrivate));
}

static void
ufo_sliding_stack_task_init(UfoSlidingStackTask *self)
{
    self->priv = UFO_SLIDING_STACK_TASK_GET_PRIVATE(self);
    self->priv->n_items = 1;
    self->priv->ordered = FALSE;
    self->priv->window = NULL;
}
