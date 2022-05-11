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

#include "ufo-flatten-inplace-task.h"

typedef enum {
    MODE_SUM,
    MODE_MIN,
    MODE_MAX,
} Mode;

static GEnumValue mode_values[] = {
    { MODE_SUM, "MODE_SUM", "sum" },
    { MODE_MIN, "MODE_MIN", "min" },
    { MODE_MAX, "MODE_MAX", "max" },
    { 0, NULL, NULL}
};

struct _UfoFlattenInplaceTaskPrivate {
    Mode mode;
    gboolean generated;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoFlattenInplaceTask, ufo_flatten_inplace_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_FLATTEN_INPLACE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FLATTEN_INPLACE_TASK, UfoFlattenInplaceTaskPrivate))

enum {
    PROP_0,
    PROP_MODE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_flatten_inplace_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_FLATTEN_INPLACE_TASK, NULL));
}

static void
ufo_flatten_inplace_task_setup (UfoTask *task,
                                UfoResources *resources,
                                GError **error)
{
}

static void
ufo_flatten_inplace_task_get_requisition (UfoTask *task,
                                          UfoBuffer **inputs,
                                          UfoRequisition *requisition,
                                          GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_flatten_inplace_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_flatten_inplace_task_get_num_dimensions (UfoTask *task,
                                             guint input)
{
    return 2;
}

static UfoTaskMode
ufo_flatten_inplace_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_REDUCTOR | UFO_TASK_MODE_CPU;
}

static gboolean
ufo_flatten_inplace_task_process (UfoTask *task,
                                  UfoBuffer **inputs,
                                  UfoBuffer *output,
                                  UfoRequisition *requisition)
{
    UfoFlattenInplaceTaskPrivate *priv;
    gfloat *in_array;
    gfloat *out_array;
    gsize n_pixels;

    priv = UFO_FLATTEN_INPLACE_TASK_GET_PRIVATE (task);
    n_pixels = requisition->dims[0] * requisition->dims[1];
    in_array = ufo_buffer_get_host_array (inputs[0], NULL);
    out_array = ufo_buffer_get_host_array (output, NULL);

    switch (priv->mode) {
        case MODE_SUM:
            for (gsize i = 0; i < n_pixels; i++)
                out_array[i] += in_array[i];
            break;

        case MODE_MIN:
            for (gsize i = 0; i < n_pixels; i++) {
                if (in_array[i] < out_array[i])
                    out_array[i] = in_array[i];
            }
            break;

        case MODE_MAX:
            for (gsize i = 0; i < n_pixels; i++) {
                if (in_array[i] > out_array[i])
                    out_array[i] = in_array[i];
            }
            break;
        default:
            break;
    }

    return TRUE;
}

static gboolean
ufo_flatten_inplace_task_generate (UfoTask *task,
                                   UfoBuffer *output,
                                   UfoRequisition *requisition)
{
    UfoFlattenInplaceTaskPrivate *priv;

    priv = UFO_FLATTEN_INPLACE_TASK_GET_PRIVATE (task);

    if (priv->generated)
        return FALSE;

    priv->generated = TRUE;
    return TRUE;
}

static void
ufo_flatten_inplace_task_set_property (GObject *object,
                                       guint property_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
    UfoFlattenInplaceTaskPrivate *priv = UFO_FLATTEN_INPLACE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_MODE:
            priv->mode = g_value_get_enum (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_flatten_inplace_task_get_property (GObject *object,
                                       guint property_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
    UfoFlattenInplaceTaskPrivate *priv = UFO_FLATTEN_INPLACE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_MODE:
            g_value_set_enum (value, priv->mode);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_flatten_inplace_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_flatten_inplace_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_flatten_inplace_task_setup;
    iface->get_num_inputs = ufo_flatten_inplace_task_get_num_inputs;
    iface->get_num_dimensions = ufo_flatten_inplace_task_get_num_dimensions;
    iface->get_mode = ufo_flatten_inplace_task_get_mode;
    iface->get_requisition = ufo_flatten_inplace_task_get_requisition;
    iface->process = ufo_flatten_inplace_task_process;
    iface->generate = ufo_flatten_inplace_task_generate;
}

static void
ufo_flatten_inplace_task_class_init (UfoFlattenInplaceTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_flatten_inplace_task_set_property;
    oclass->get_property = ufo_flatten_inplace_task_get_property;
    oclass->finalize = ufo_flatten_inplace_task_finalize;

    properties[PROP_MODE] =
        g_param_spec_enum ("mode",
            "Mode (min, max, sum)",
            "Mode (min, max, sum)",
            g_enum_register_static ("ufo_flatten_inplace_mode", mode_values),
            MODE_SUM, G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoFlattenInplaceTaskPrivate));
}

static void
ufo_flatten_inplace_task_init(UfoFlattenInplaceTask *self)
{
    self->priv = UFO_FLATTEN_INPLACE_TASK_GET_PRIVATE(self);
    self->priv->mode = MODE_SUM;
    self->priv->generated = FALSE;
}
