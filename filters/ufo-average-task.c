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

#include <string.h>
#include <gmodule.h>

#include "ufo-average-task.h"


struct _UfoAverageTaskPrivate {
    gfloat *averaged;
    gboolean is_data_averaged;
    guint counter;
    guint n_generate;
};

enum {
    PROP_0,
    PROP_NUM_GENERATE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoAverageTask, ufo_average_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_AVERAGE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_AVERAGE_TASK, UfoAverageTaskPrivate))


UfoNode *
ufo_average_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_AVERAGE_TASK, NULL));
}

static void
ufo_average_task_setup (UfoTask *task,
                         UfoResources *resources,
                         GError **error)
{
}

static void
ufo_average_task_get_requisition (UfoTask *task,
                                  UfoBuffer **inputs,
                                  UfoRequisition *requisition,
                                  GError **error)
{
    UfoAverageTaskPrivate *priv;

    priv = UFO_AVERAGE_TASK_GET_PRIVATE (UFO_AVERAGE_TASK (task));
    ufo_buffer_get_requisition (inputs[0], requisition);

    if (priv->averaged == NULL) {
        priv->averaged = g_malloc0 (requisition->dims[0] *
                                    requisition->dims[1] * sizeof (gfloat));
    }
}

static guint
ufo_average_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_average_task_get_num_dimensions (UfoTask *task,
                               guint input)
{
    g_return_val_if_fail (input == 0, 0);
    return 2;
}

static UfoTaskMode
ufo_average_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_REDUCTOR | UFO_TASK_MODE_CPU;
}

static gboolean
ufo_average_task_process (UfoTask *task,
                           UfoBuffer **inputs,
                           UfoBuffer *output,
                           UfoRequisition *requisition)
{
    UfoAverageTaskPrivate *priv;
    gfloat *in_array;
    gfloat *out_array;
    gsize n_pixels;

    priv = UFO_AVERAGE_TASK_GET_PRIVATE (UFO_AVERAGE_TASK (task));
    n_pixels = requisition->dims[0] * requisition->dims[1];
    in_array = ufo_buffer_get_host_array (inputs[0], NULL);
    out_array = ufo_buffer_get_host_array (output, NULL);

    for (gsize i = 0; i < n_pixels; i++)
        out_array[i] += in_array[i];

    priv->counter++;
    return TRUE;
}

static gboolean
ufo_average_task_generate (UfoTask *task,
                            UfoBuffer *output,
                            UfoRequisition *requisition)
{
    UfoAverageTaskPrivate *priv;
    gfloat *out_array;
    gsize n_pixels;

    priv = UFO_AVERAGE_TASK_GET_PRIVATE (UFO_AVERAGE_TASK (task));

    if (priv->n_generate == 0)
        return FALSE;

    out_array = ufo_buffer_get_host_array (output, NULL);
    n_pixels = requisition->dims[0] * requisition->dims[1];

    if (!priv->is_data_averaged) {
        for (gsize i = 0; i < n_pixels; i++)
            priv->averaged[i] = out_array[i] / (gfloat) priv->counter;

        priv->is_data_averaged = TRUE;
    }

    g_memmove (out_array, priv->averaged, n_pixels * sizeof (float));
    priv->n_generate--;

    return TRUE;
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_average_task_setup;
    iface->get_num_inputs = ufo_average_task_get_num_inputs;
    iface->get_num_dimensions = ufo_average_task_get_num_dimensions;
    iface->get_mode = ufo_average_task_get_mode;
    iface->get_requisition = ufo_average_task_get_requisition;
    iface->process = ufo_average_task_process;
    iface->generate = ufo_average_task_generate;
}

static void
ufo_average_task_finalize (GObject *object)
{
    UfoAverageTaskPrivate *priv;

    priv = UFO_AVERAGE_TASK_GET_PRIVATE (object);

    if (priv->averaged != NULL) {
        g_free (priv->averaged);
        priv->averaged = NULL;
    }
}

static void
ufo_average_task_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
    UfoAverageTaskPrivate *priv = UFO_AVERAGE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_GENERATE:
            priv->n_generate = g_value_get_uint (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_average_task_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
    UfoAverageTaskPrivate *priv = UFO_AVERAGE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_GENERATE:
            g_value_set_uint (value, priv->n_generate);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_average_task_class_init (UfoAverageTaskClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = ufo_average_task_finalize;
    oclass->set_property = ufo_average_task_set_property;
    oclass->get_property = ufo_average_task_get_property;

    properties[PROP_NUM_GENERATE] =
        g_param_spec_uint ("number",
            "Number of averaged images to generate",
            "Number of averaged images to generate",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (G_OBJECT_CLASS (klass), sizeof(UfoAverageTaskPrivate));
}

static void
ufo_average_task_init(UfoAverageTask *self)
{
    self->priv = UFO_AVERAGE_TASK_GET_PRIVATE(self);
    self->priv->counter = 0;
    self->priv->averaged = NULL;
    self->priv->n_generate = 1;
    self->priv->is_data_averaged = FALSE;
}
