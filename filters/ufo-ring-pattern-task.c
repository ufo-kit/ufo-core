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

#include <math.h>
#include <stdlib.h>

#include "ufo-ring-pattern-task.h"
#include "ufo-priv.h"


struct _UfoRingPatternTaskPrivate {
    unsigned ring_thickness;
    unsigned ring_end;
    unsigned ring_start;
    unsigned ring_current;
    unsigned ring_step;
    unsigned width;
    unsigned height;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoRingPatternTask, ufo_ring_pattern_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_RING_PATTERN_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_RING_PATTERN_TASK, UfoRingPatternTaskPrivate))

enum {
    PROP_0,
    PROP_RING_START,
    PROP_RING_STEP,
    PROP_RING_END,
    PROP_RING_THICKNESS,
    PROP_WIDTH,
    PROP_HEIGHT,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_ring_pattern_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_RING_PATTERN_TASK, NULL));
}

static void
ufo_ring_pattern_task_setup (UfoTask *task,
                       UfoResources *resources,
                       GError **error)
{
}

static void
ufo_ring_pattern_task_get_requisition (UfoTask *task,
                                       UfoBuffer **inputs,
                                       UfoRequisition *requisition,
                                       GError **error)
{
    UfoRingPatternTaskPrivate *priv = UFO_RING_PATTERN_TASK_GET_PRIVATE (task);
    requisition->dims[0] = priv->width;
    requisition->dims[1] = priv->height;
    requisition->n_dims = 2;
}

static guint
ufo_ring_pattern_task_get_num_inputs (UfoTask *task)
{
    return 0;
}

static guint
ufo_ring_pattern_task_get_num_dimensions (UfoTask *task,
                                             guint input)
{
    return 0;
}

static UfoTaskMode
ufo_ring_pattern_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_GENERATOR | UFO_TASK_MODE_CPU;
}

static void add_ring_metadata(UfoBuffer *output, unsigned number_ones,
                              unsigned radius)
{
    GValue value = G_VALUE_INIT;
    g_value_init(&value, G_TYPE_UINT);

    g_value_set_uint(&value, number_ones);
    ufo_buffer_set_metadata (output, "number_ones", &value);

    g_value_set_uint(&value, radius);
    ufo_buffer_set_metadata (output, "radius", &value);
}

static gboolean
ufo_ring_pattern_task_generate (UfoTask *task,
                                UfoBuffer *output,
                                UfoRequisition *requisition)
{
    UfoRingPatternTaskPrivate *priv = UFO_RING_PATTERN_TASK_GET_PRIVATE (task);

    if (priv->ring_current > priv->ring_end)
        return FALSE;

    float *out = ufo_buffer_get_host_array (output, NULL);
    int dimx = (int) priv->width;
    int dimy = (int) priv->height;
    unsigned counter = 0;

    for (int y = -(dimy / 2); y < dimy / 2; ++y) {
        for (int x = -(dimx / 2); x < dimx / 2; ++x) {
            double dist = sqrt (x * x + y * y) - priv->ring_current;
            dist = dist < 0 ? -dist : dist;
            if (dist < (double) priv->ring_thickness / 2) {
                out[(x + (dimx)) % dimx + ((y + (dimy)) % dimy) * dimx] = 1;
                ++counter;
            }
            else
                out[(x + (dimx)) % dimx + ((y + (dimy)) % dimy) * dimx] = 0;
        }
    }

    add_ring_metadata(output, counter, priv->ring_current);

    for (int y = -(dimy / 2); y < dimy / 2; ++y) {
        for (int x = -(dimx / 2); x < dimx / 2; ++x) {
            double dist = sqrt (x * x + y * y) - priv->ring_current;
            dist = dist < 0 ? -dist : dist;
            if (dist < (double) priv->ring_thickness / 2) {
                out[(x + (dimx)) % dimx + ((y + (dimy)) % dimy) * dimx] /=
                    (float) counter;
            }
            else
                out[(x + (dimx)) % dimx + ((y + (dimy)) % dimy) * dimx] = 0;
        }
    }

    priv->ring_current += priv->ring_step;
    return TRUE;
}

static void
ufo_ring_pattern_task_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    UfoRingPatternTaskPrivate *priv = UFO_RING_PATTERN_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_RING_START:
            priv->ring_start = g_value_get_uint(value);
            priv->ring_current = g_value_get_uint(value);
            break;
        case PROP_RING_STEP:
            priv->ring_step = g_value_get_uint(value);
            break;
        case PROP_RING_END:
            priv->ring_end = g_value_get_uint(value);
            break;
        case PROP_RING_THICKNESS:
            priv->ring_thickness = g_value_get_uint(value);
            break;
        case PROP_WIDTH:
            priv->width = ceil_power_of_two (g_value_get_uint (value));
            break;
        case PROP_HEIGHT:
            priv->height = ceil_power_of_two (g_value_get_uint (value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_ring_pattern_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoRingPatternTaskPrivate *priv = UFO_RING_PATTERN_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_RING_START:
            g_value_set_uint (value, priv->ring_start);
            break;
        case PROP_RING_STEP:
            g_value_set_uint (value, priv->ring_step);
            break;
        case PROP_RING_END:
            g_value_set_uint (value, priv->ring_end);
            break;
        case PROP_RING_THICKNESS:
            g_value_set_uint (value, priv->ring_thickness);
            break;
        case PROP_WIDTH:
            g_value_set_uint (value, priv->width);
            break;
        case PROP_HEIGHT:
            g_value_set_uint (value, priv->height);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_ring_pattern_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_ring_pattern_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_ring_pattern_task_setup;
    iface->get_num_inputs = ufo_ring_pattern_task_get_num_inputs;
    iface->get_num_dimensions = ufo_ring_pattern_task_get_num_dimensions;
    iface->get_mode = ufo_ring_pattern_task_get_mode;
    iface->get_requisition = ufo_ring_pattern_task_get_requisition;
    iface->generate = ufo_ring_pattern_task_generate;
}

static void
ufo_ring_pattern_task_class_init (UfoRingPatternTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_ring_pattern_task_set_property;
    gobject_class->get_property = ufo_ring_pattern_task_get_property;
    gobject_class->finalize = ufo_ring_pattern_task_finalize;

    properties[PROP_RING_START] =
        g_param_spec_uint ("ring-start",
            "give starting radius size",
            "give starting radius size",
            1, G_MAXUINT, 5,
            G_PARAM_READWRITE);

    properties[PROP_RING_STEP] =
        g_param_spec_uint ("ring-step",
            "Gives ring step",
            "Gives ring step",
            1, G_MAXUINT, 2,
            G_PARAM_READWRITE);

    properties[PROP_RING_END] =
        g_param_spec_uint ("ring-end",
            "give ending radius size",
            "give ending radius size",
            1, G_MAXUINT, 5,
            G_PARAM_READWRITE);

    properties[PROP_RING_THICKNESS] =
        g_param_spec_uint ("ring-thickness",
            "give desired ring thickness",
            "give desired ring thickness",
            1, G_MAXUINT, 13,
            G_PARAM_READWRITE);

    properties[PROP_WIDTH] =
        g_param_spec_uint ("width",
            "Give x size of output image",
            "Give x size of output image",
            1, G_MAXUINT, 1024,
            G_PARAM_READWRITE);

    properties[PROP_HEIGHT] =
        g_param_spec_uint ("height",
            "Give y size of output image",
            "Give y size of output image",
            1, G_MAXUINT, 1024,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoRingPatternTaskPrivate));
}

static void
ufo_ring_pattern_task_init(UfoRingPatternTask *self)
{
    self->priv = UFO_RING_PATTERN_TASK_GET_PRIVATE(self);
    self->priv->ring_start = 5;
    self->priv->ring_end = 5;
    self->priv->ring_thickness = 13;
    self->priv->ring_current = self->priv->ring_start;
    self->priv->width = 1024;
    self->priv->height = 1024;
    self->priv->ring_step = 2;
}
