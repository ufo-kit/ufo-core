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

#include <math.h>
#include <string.h>
#include "ufo-map-slice-task.h"

struct _UfoMapSliceTaskPrivate {
    guint number;
    guint current;
    guint grid_size;    /* in number of images, not pixel */
    gsize input_width;
    gsize input_height;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoMapSliceTask, ufo_map_slice_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_MAP_SLICE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_MAP_SLICE_TASK, UfoMapSliceTaskPrivate))

enum {
    PROP_0,
    PROP_NUMBER,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_map_slice_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_MAP_SLICE_TASK, NULL));
}

static void
ufo_map_slice_task_setup (UfoTask *task,
                          UfoResources *resources,
                          GError **error)
{
}

static void
ufo_map_slice_task_get_requisition (UfoTask *task,
                                    UfoBuffer **inputs,
                                    UfoRequisition *requisition,
                                    GError **error)
{
    UfoMapSliceTaskPrivate *priv;
    UfoRequisition in_req;

    priv = UFO_MAP_SLICE_TASK_GET_PRIVATE (task);

    ufo_buffer_get_requisition (inputs[0], &in_req);
    requisition->n_dims = 2;

    /* We are safe to assume that "number" is set alright by now */
    requisition->dims[0] = priv->grid_size * in_req.dims[0];
    requisition->dims[1] = priv->grid_size * in_req.dims[1];

    priv->input_width = in_req.dims[0];
    priv->input_height = in_req.dims[1];
}

static guint
ufo_map_slice_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_map_slice_task_get_num_dimensions (UfoTask *task,
                                       guint input)
{
    return 2;
}

static UfoTaskMode
ufo_map_slice_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_REDUCTOR;
}

static gboolean
ufo_map_slice_task_process (UfoTask *task,
                            UfoBuffer **inputs,
                            UfoBuffer *output,
                            UfoRequisition *requisition)
{
    UfoMapSliceTaskPrivate *priv;
    gfloat *src;
    gfloat *dst;
    gsize x, y;

    priv = UFO_MAP_SLICE_TASK_GET_PRIVATE (task);

    src = ufo_buffer_get_host_array (inputs[0], NULL);
    dst = ufo_buffer_get_host_array (output, NULL);
    y = (guint) floor (priv->current / priv->grid_size);
    x = priv->current - y * priv->grid_size;

    x *= priv->input_width;
    y *= priv->input_height;

    if (priv->current == 0)
        memset (dst, 0, ufo_buffer_get_size (output));

    /* Copy rows from src to dst */
    for (gsize i = 0; i < priv->input_height; i++) {
        memcpy (&dst[(y + i) * requisition->dims[0] + x],
                &src[i * priv->input_width],
                sizeof (gfloat) * priv->input_width);
    }

    priv->current++;

    return priv->current < priv->number;
}

static gboolean
ufo_map_slice_task_generate (UfoTask *task,
                             UfoBuffer *output,
                             UfoRequisition *requisition)
{
    UfoMapSliceTaskPrivate *priv;

    priv = UFO_MAP_SLICE_TASK_GET_PRIVATE (task);

    if (priv->current > 0) {
        priv->current = 0;
        return TRUE;
    }

    return FALSE;
}

static void
ufo_map_slice_task_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
    UfoMapSliceTaskPrivate *priv = UFO_MAP_SLICE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUMBER:
            priv->number = g_value_get_uint (value);
            priv->grid_size = (guint) ceil (sqrt (priv->number));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_map_slice_task_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
    UfoMapSliceTaskPrivate *priv = UFO_MAP_SLICE_TASK_GET_PRIVATE (object);

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
ufo_map_slice_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_map_slice_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_map_slice_task_setup;
    iface->get_num_inputs = ufo_map_slice_task_get_num_inputs;
    iface->get_num_dimensions = ufo_map_slice_task_get_num_dimensions;
    iface->get_mode = ufo_map_slice_task_get_mode;
    iface->get_requisition = ufo_map_slice_task_get_requisition;
    iface->process = ufo_map_slice_task_process;
    iface->generate = ufo_map_slice_task_generate;
}

static void
ufo_map_slice_task_class_init (UfoMapSliceTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_map_slice_task_set_property;
    oclass->get_property = ufo_map_slice_task_get_property;
    oclass->finalize = ufo_map_slice_task_finalize;

    properties[PROP_NUMBER] =
        g_param_spec_uint ("number",
            "Number of expected images",
            "Number of expected images",
            1, G_MAXUINT, 1, G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoMapSliceTaskPrivate));
}

static void
ufo_map_slice_task_init(UfoMapSliceTask *self)
{
    self->priv = UFO_MAP_SLICE_TASK_GET_PRIVATE(self);
    self->priv->number = 1;
    self->priv->current = 0;
    self->priv->grid_size = 1;
}
