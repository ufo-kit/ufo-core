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

#include <stdio.h>
#include "ufo-stdin-task.h"


struct _UfoStdinTaskPrivate {
    gsize width;
    gsize height;
    gsize bytes_per_pixel;
    UfoBufferDepth bitdepth;
    gboolean convert;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoStdinTask, ufo_stdin_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_STDIN_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_STDIN_TASK, UfoStdinTaskPrivate))

enum {
    PROP_0,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_BITDEPTH,
    PROP_CONVERT,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_stdin_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_STDIN_TASK, NULL));
}

static void
ufo_stdin_task_setup (UfoTask *task,
                      UfoResources *resources,
                      GError **error)
{
}

static void
ufo_stdin_task_get_requisition (UfoTask *task,
                                UfoBuffer **inputs,
                                UfoRequisition *requisition,
                                GError **error)
{
    UfoStdinTaskPrivate *priv;

    priv = UFO_STDIN_TASK_GET_PRIVATE (task);
    requisition->n_dims = 2;
    requisition->dims[0] = priv->width;
    requisition->dims[1] = priv->height;
}

static guint
ufo_stdin_task_get_num_inputs (UfoTask *task)
{
    return 0;
}

static guint
ufo_stdin_task_get_num_dimensions (UfoTask *task,
                                   guint input)
{
    return 2;
}

static UfoTaskMode
ufo_stdin_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_GENERATOR | UFO_TASK_MODE_CPU;
}

static gboolean
ufo_stdin_task_generate (UfoTask *task,
                         UfoBuffer *output,
                         UfoRequisition *requisition)
{
    UfoStdinTaskPrivate *priv;
    gchar *data;
    gboolean succeeded;

    priv = UFO_STDIN_TASK_GET_PRIVATE (task);
    data = (gchar *) ufo_buffer_get_host_array (output, NULL);
    succeeded = fread (data, priv->bytes_per_pixel * priv->width * priv->height, 1, stdin) == 1;

    if (succeeded && priv->convert && priv->bitdepth != UFO_BUFFER_DEPTH_32F)
        ufo_buffer_convert (output, priv->bitdepth);

    return succeeded;
}

static void
ufo_stdin_task_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    UfoStdinTaskPrivate *priv = UFO_STDIN_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_WIDTH:
            priv->width = (gsize) g_value_get_uint (value);
            break;
        case PROP_HEIGHT:
            priv->height = (gsize) g_value_get_uint (value);
            break;
        case PROP_BITDEPTH:
            switch (g_value_get_uint (value)) {
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
                    g_warning ("Cannot set bitdepth other than 8, 16 or 32.");
            }
            break;
        case PROP_CONVERT:
            priv->convert = g_value_get_boolean (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_stdin_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoStdinTaskPrivate *priv = UFO_STDIN_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_WIDTH:
            g_value_set_uint (value, (guint) priv->width);
            break;
        case PROP_HEIGHT:
            g_value_set_uint (value, (guint) priv->height);
            break;
        case PROP_BITDEPTH:
            g_value_set_uint (value, priv->bitdepth);
            break;
        case PROP_CONVERT:
            g_value_set_boolean (value, priv->convert);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_stdin_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_stdin_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_stdin_task_setup;
    iface->get_num_inputs = ufo_stdin_task_get_num_inputs;
    iface->get_num_dimensions = ufo_stdin_task_get_num_dimensions;
    iface->get_mode = ufo_stdin_task_get_mode;
    iface->get_requisition = ufo_stdin_task_get_requisition;
    iface->generate = ufo_stdin_task_generate;
}

static void
ufo_stdin_task_class_init (UfoStdinTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_stdin_task_set_property;
    oclass->get_property = ufo_stdin_task_get_property;
    oclass->finalize = ufo_stdin_task_finalize;

    properties[PROP_WIDTH] =
        g_param_spec_uint("width",
            "Width of raw image",
            "Width of raw image",
            0, G_MAXUINT, G_MAXUINT,
            G_PARAM_READWRITE);

    properties[PROP_HEIGHT] =
        g_param_spec_uint("height",
            "Height of raw image",
            "Height of raw image",
            0, G_MAXUINT, G_MAXUINT,
            G_PARAM_READWRITE);

    properties[PROP_BITDEPTH] =
        g_param_spec_uint("bitdepth",
            "Bitdepth of raw image",
            "Bitdepth of raw image",
            0, G_MAXUINT, G_MAXUINT,
            G_PARAM_READWRITE);

    properties[PROP_CONVERT] =
        g_param_spec_boolean("convert",
            "Enable automatic conversion",
            "Enable automatic conversion of input data types to float",
            TRUE,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof (UfoStdinTaskPrivate));
}

static void
ufo_stdin_task_init(UfoStdinTask *self)
{
    self->priv = UFO_STDIN_TASK_GET_PRIVATE(self);
    self->priv->width = 0;
    self->priv->height = 0;
    self->priv->bitdepth = UFO_BUFFER_DEPTH_32F;
    self->priv->convert = TRUE;
}
