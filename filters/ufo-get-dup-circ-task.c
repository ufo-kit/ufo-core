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
 *
 * Authored by: Alexandre Lewkowicz (lewkow_a@epita.fr)
 */
#include "config.h"

#include <math.h> /* sqrt */
#include <string.h> /* memcpy */

#include "ufo-get-dup-circ-task.h"
#include "ufo-ring-coordinates.h"

struct _UfoGetDupCircTaskPrivate {
    float threshold;
    UfoRingCoordinate *coord;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoGetDupCircTask, ufo_get_dup_circ_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_GET_DUP_CIRC_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GET_DUP_CIRC_TASK, UfoGetDupCircTaskPrivate))

enum {
    PROP_0,
    PROP_THRESHOLD,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_get_dup_circ_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_GET_DUP_CIRC_TASK, NULL));
}

static void
ufo_get_dup_circ_task_setup (UfoTask *task,
                             UfoResources *resources,
                             GError **error)
{
}

static void
ufo_get_dup_circ_task_get_requisition (UfoTask *task,
                                       UfoBuffer **inputs,
                                       UfoRequisition *requisition,
                                       GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_get_dup_circ_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_get_dup_circ_task_get_num_dimensions (UfoTask *task,
                                          guint input)
{
    return 1;
}

static UfoTaskMode
ufo_get_dup_circ_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_CPU;
}

static void
merge_rings (UfoBuffer *output, UfoGetDupCircTaskPrivate *priv, unsigned nb_elt)
{
    float *res = ufo_buffer_get_host_array (output, NULL);
    UfoRingCoordinate *coord = (UfoRingCoordinate *) (res + 1);
    unsigned nb_coord = 0;
    for (unsigned i = 0; i < nb_elt; ++i) {
        /* We set rings to a radius of 0 when they have already been merged */
        if (priv->coord[i].r == 0)
            continue;
        coord[nb_coord] = priv->coord[i];
        float nb_merged_elt = 1;
        for (unsigned j = i + 1; j < nb_elt; ++j) {
            if (priv->coord[j].r == 0)
                continue;
            float distance = sqrtf ((priv->coord[j].x - priv->coord[i].x) *
                                    (priv->coord[j].x - priv->coord[i].x) +
                                    (priv->coord[j].y - priv->coord[i].y) *
                                    (priv->coord[j].y - priv->coord[i].y));
            float radius_diff = priv->coord[j].r - priv->coord[i].r;
            radius_diff = radius_diff < 0 ? -radius_diff : radius_diff;
            if (distance < priv->threshold && radius_diff < priv->threshold) {
                coord[nb_coord].x += priv->coord[j].x;
                coord[nb_coord].y += priv->coord[j].y;
                coord[nb_coord].r += priv->coord[j].r;
                /* Say that the j-th ring has been merged */
                priv->coord[j].r = 0;
                ++nb_merged_elt;
            }
        }
        coord[nb_coord].x /= nb_merged_elt;
        coord[nb_coord].y /= nb_merged_elt;
        coord[nb_coord].r /= nb_merged_elt;
        /* When merging occurs, search again for new candidates */
        if (nb_merged_elt > 1) {
            priv->coord[i] = coord[nb_coord];
            --i;
        }
        else
            ++nb_coord;
    }
    *res = (float) nb_coord;
}

static gboolean
ufo_get_dup_circ_task_process (UfoTask *task,
                               UfoBuffer **inputs,
                               UfoBuffer *output,
                               UfoRequisition *requisition)
{
    UfoGetDupCircTaskPrivate *priv = UFO_GET_DUP_CIRC_TASK_GET_PRIVATE (task);
    float *input = ufo_buffer_get_host_array (inputs[0], NULL);
    unsigned nb_elt = (unsigned) *input;
    UfoRingCoordinate *coord = (UfoRingCoordinate *) (input + 1);
    priv->coord = g_malloc (sizeof (UfoRingCoordinate) * nb_elt);
    memcpy (priv->coord, coord, nb_elt * sizeof (UfoRingCoordinate));
    merge_rings (output, priv, nb_elt);
    return TRUE;
}

static void
ufo_get_dup_circ_task_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
    UfoGetDupCircTaskPrivate *priv = UFO_GET_DUP_CIRC_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_THRESHOLD:
            priv->threshold = g_value_get_float (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_get_dup_circ_task_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
    UfoGetDupCircTaskPrivate *priv = UFO_GET_DUP_CIRC_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_THRESHOLD:
            g_value_set_float (value, priv->threshold);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_get_dup_circ_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_get_dup_circ_task_parent_class)->finalize (object);
    UfoGetDupCircTaskPrivate *priv = UFO_GET_DUP_CIRC_TASK_GET_PRIVATE (object);
    g_free (priv->coord);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_get_dup_circ_task_setup;
    iface->get_num_inputs = ufo_get_dup_circ_task_get_num_inputs;
    iface->get_num_dimensions = ufo_get_dup_circ_task_get_num_dimensions;
    iface->get_mode = ufo_get_dup_circ_task_get_mode;
    iface->get_requisition = ufo_get_dup_circ_task_get_requisition;
    iface->process = ufo_get_dup_circ_task_process;
}

static void
ufo_get_dup_circ_task_class_init (UfoGetDupCircTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_get_dup_circ_task_set_property;
    gobject_class->get_property = ufo_get_dup_circ_task_get_property;
    gobject_class->finalize = ufo_get_dup_circ_task_finalize;

    properties[PROP_THRESHOLD] =
        g_param_spec_float ("threshold",
            "Give maximum ring distance, and radius difference",
            "Give maximum ring distance, and radius difference",
            1, G_MAXFLOAT, 10,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoGetDupCircTaskPrivate));
}

static void
ufo_get_dup_circ_task_init(UfoGetDupCircTask *self)
{
    self->priv = UFO_GET_DUP_CIRC_TASK_GET_PRIVATE(self);
    self->priv->threshold = 10;
    self->priv->coord = NULL;
}
