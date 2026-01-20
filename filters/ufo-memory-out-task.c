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
#include "ufo-memory-out-task.h"


struct _UfoMemoryOutTaskPrivate {
    gfloat *pointer;
    gsize   max_size;
    gsize   written;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoMemoryOutTask, ufo_memory_out_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_MEMORY_OUT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_MEMORY_OUT_TASK, UfoMemoryOutTaskPrivate))

enum {
    PROP_0,
    PROP_POINTER,
    PROP_MAX_SIZE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_memory_out_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_MEMORY_OUT_TASK, NULL));
}

static void
ufo_memory_out_task_setup (UfoTask *task,
                           UfoResources *resources,
                           GError **error)
{
    UfoMemoryOutTaskPrivate *priv;

    priv = UFO_MEMORY_OUT_TASK_GET_PRIVATE (task);

    if (priv->pointer == NULL) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP, "`pointer' property not set");
        return;
    }

    if (priv->max_size < 4) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP, "`max-size' property < 4");
        return;
    }

    priv->written = 0;
}

static void
ufo_memory_out_task_get_requisition (UfoTask *task,
                                     UfoBuffer **inputs,
                                     UfoRequisition *requisition,
                                     GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_memory_out_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_memory_out_task_get_num_dimensions (UfoTask *task,
                                        guint input)
{
    return 2;
}

static UfoTaskMode
ufo_memory_out_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_SINK | UFO_TASK_MODE_CPU;
}

static gboolean
ufo_memory_out_task_process (UfoTask *task,
                             UfoBuffer **inputs,
                             UfoBuffer *output,
                             UfoRequisition *requisition)
{
    UfoMemoryOutTaskPrivate *priv;
    gfloat *in_mem;
    gchar *out_mem;
    gsize size;

    priv = UFO_MEMORY_OUT_TASK_GET_PRIVATE (task);

    if (priv->written >= priv->max_size) {
        g_warning ("Already written %zu bytes, cannot append more", priv->written);
        return FALSE;
    }

    size = ufo_buffer_get_size (inputs[0]);
    size = MIN (size, priv->max_size - priv->written);
    out_mem = (gchar *) priv->pointer;

    in_mem = ufo_buffer_get_host_array (inputs[0], NULL);
    memcpy (&out_mem[priv->written], in_mem, size);
    priv->written += size;

    return TRUE;
}

static void
ufo_memory_out_task_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
    UfoMemoryOutTaskPrivate *priv = UFO_MEMORY_OUT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_POINTER:
            priv->pointer = (gpointer) g_value_get_ulong (value);
            break;
        case PROP_MAX_SIZE:
            priv->max_size = (gsize) g_value_get_ulong (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_memory_out_task_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
    UfoMemoryOutTaskPrivate *priv = UFO_MEMORY_OUT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_POINTER:
            g_value_set_ulong (value, (gulong) priv->pointer);
            break;
        case PROP_MAX_SIZE:
            g_value_set_ulong (value, (gsize) priv->max_size);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_memory_out_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_memory_out_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_memory_out_task_setup;
    iface->get_num_inputs = ufo_memory_out_task_get_num_inputs;
    iface->get_num_dimensions = ufo_memory_out_task_get_num_dimensions;
    iface->get_mode = ufo_memory_out_task_get_mode;
    iface->get_requisition = ufo_memory_out_task_get_requisition;
    iface->process = ufo_memory_out_task_process;
}

static void
ufo_memory_out_task_class_init (UfoMemoryOutTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_memory_out_task_set_property;
    oclass->get_property = ufo_memory_out_task_get_property;
    oclass->finalize = ufo_memory_out_task_finalize;

    properties[PROP_POINTER] =
        g_param_spec_ulong ("pointer",
            "Pointer to pre-allocated memory",
            "Pointer to pre-allocated memory",
            0, G_MAXULONG, 0,
            G_PARAM_READWRITE);

    properties[PROP_MAX_SIZE] =
        g_param_spec_ulong ("max-size",
            "Maximum writable size",
            "Maximum writable size",
            0, G_MAXULONG, 0,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoMemoryOutTaskPrivate));
}

static void
ufo_memory_out_task_init(UfoMemoryOutTask *self)
{
    self->priv = UFO_MEMORY_OUT_TASK_GET_PRIVATE(self);
    self->priv->pointer = NULL;
    self->priv->max_size = 0;
    self->priv->written = 0;
}
