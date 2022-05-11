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

#include "ufo-memory-in-task.h"


struct _UfoMemoryInTaskPrivate {
    gfloat *pointer;
    guint   width;
    guint   height;
    gsize     bytes_per_pixel;
    UfoBufferDepth   bitdepth;
    guint   number;
    guint   read;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoMemoryInTask, ufo_memory_in_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_MEMORY_IN_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_MEMORY_IN_TASK, UfoMemoryInTaskPrivate))

enum {
    PROP_0,
    PROP_POINTER,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_BITDEPTH,
    PROP_NUMBER,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_memory_in_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_MEMORY_IN_TASK, NULL));
}

static void
ufo_memory_in_task_setup (UfoTask *task,
                          UfoResources *resources,
                          GError **error)
{
    UfoMemoryInTaskPrivate *priv;

    priv = UFO_MEMORY_IN_TASK_GET_PRIVATE (task);

    if (priv->pointer == NULL)
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP, "`pointer' property not set");

    priv->read = 0;
}

static void
ufo_memory_in_task_get_requisition (UfoTask *task,
                                    UfoBuffer **inputs,
                                    UfoRequisition *requisition,
                                    GError **error)
{
    UfoMemoryInTaskPrivate *priv;

    priv = UFO_MEMORY_IN_TASK_GET_PRIVATE (task);
    requisition->n_dims = 2;
    requisition->dims[0] = priv->width;
    requisition->dims[1] = priv->height;
}

static guint
ufo_memory_in_task_get_num_inputs (UfoTask *task)
{
    return 0;
}

static guint
ufo_memory_in_task_get_num_dimensions (UfoTask *task,
                                       guint input)
{
    return 0;
}

static UfoTaskMode
ufo_memory_in_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_GENERATOR | UFO_TASK_MODE_CPU;
}

static gboolean
ufo_memory_in_task_generate (UfoTask *task,
                             UfoBuffer *output,
                             UfoRequisition *requisition)
{
    UfoMemoryInTaskPrivate *priv;
    guint8 *data;

    priv = UFO_MEMORY_IN_TASK_GET_PRIVATE (task);

    if (priv->read == priv->number)
        return FALSE;

    data = (guint8 *) ufo_buffer_get_host_array (output, NULL);
    memcpy (data, &priv->pointer[priv->read * priv->width * priv->height], priv->width * priv->height * priv->bytes_per_pixel);

    if (priv->bitdepth != UFO_BUFFER_DEPTH_32F)
        ufo_buffer_convert (output, priv->bitdepth);

    priv->read++;

    return TRUE;
}

static void
ufo_memory_in_task_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
    UfoMemoryInTaskPrivate *priv = UFO_MEMORY_IN_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_POINTER:
            priv->pointer = (gpointer) g_value_get_ulong (value);
            break;
        case PROP_WIDTH:
            priv->width = g_value_get_uint (value);
            break;
        case PROP_HEIGHT:
            priv->height = g_value_get_uint (value);
            break;
        case PROP_NUMBER:
            priv->number = g_value_get_uint (value);
            break;
        case PROP_BITDEPTH:
            switch(g_value_get_uint(value)){
                case 8:
                    priv->bitdepth = UFO_BUFFER_DEPTH_8U;
                    priv->bytes_per_pixel = 1;
                    break;
                case 16:
                    priv->bitdepth = UFO_BUFFER_DEPTH_16U;
                    priv->bytes_per_pixel = 2;
                    break;
                case 32:
                    priv->bitdepth = UFO_BUFFER_DEPTH_32F;
                    priv->bytes_per_pixel = 4;
                    break;
                default:
                    g_warning("Cannot set bitdepth other than 8, 16, 32.");
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_memory_in_task_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
    UfoMemoryInTaskPrivate *priv = UFO_MEMORY_IN_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_POINTER:
            g_value_set_ulong (value, (gulong) priv->pointer);
            break;
        case PROP_WIDTH:
            g_value_set_uint (value, priv->width);
            break;
        case PROP_HEIGHT:
            g_value_set_uint (value, priv->height);
            break;
        case PROP_BITDEPTH:
            g_value_set_uint (value, priv->bitdepth);
            break;
        case PROP_NUMBER:
            g_value_set_uint (value, priv->number);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_memory_in_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_memory_in_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_memory_in_task_setup;
    iface->get_num_inputs = ufo_memory_in_task_get_num_inputs;
    iface->get_num_dimensions = ufo_memory_in_task_get_num_dimensions;
    iface->get_mode = ufo_memory_in_task_get_mode;
    iface->get_requisition = ufo_memory_in_task_get_requisition;
    iface->generate = ufo_memory_in_task_generate;
}

static void
ufo_memory_in_task_class_init (UfoMemoryInTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_memory_in_task_set_property;
    oclass->get_property = ufo_memory_in_task_get_property;
    oclass->finalize = ufo_memory_in_task_finalize;

    properties[PROP_POINTER] =
        g_param_spec_ulong ("pointer",
            "Pointer to pre-allocated memory",
            "Pointer to pre-allocated memory",
            0, G_MAXULONG, 0,
            G_PARAM_READWRITE);

    properties[PROP_WIDTH] =
        g_param_spec_uint ("width",
            "Width of the buffer",
            "Width of the buffer",
            1, 2 << 16, 1,
            G_PARAM_READWRITE);

    properties[PROP_HEIGHT] =
        g_param_spec_uint ("height",
            "Height of the buffer",
            "Height of the buffer",
            1, 2 << 16, 1,
            G_PARAM_READWRITE);

    properties[PROP_BITDEPTH] =
        g_param_spec_uint("bitdepth",
            "Bitdepth of the buffer",
            "Bitdepth of the buffer",
            0, G_MAXUINT, G_MAXUINT,
            G_PARAM_READWRITE);

    properties[PROP_NUMBER] =
        g_param_spec_uint ("number",
            "Number of buffers",
            "Number of buffers",
            1, 2 << 16, 1,
            G_PARAM_READWRITE);


    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoMemoryInTaskPrivate));
}

static void
ufo_memory_in_task_init(UfoMemoryInTask *self)
{
    self->priv = UFO_MEMORY_IN_TASK_GET_PRIVATE(self);
    self->priv->pointer = NULL;
    self->priv->width = 1;
    self->priv->height = 1;
    self->priv->bitdepth = UFO_BUFFER_DEPTH_32F;
    self->priv->number = 0;
}
