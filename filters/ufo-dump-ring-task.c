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

#include "ufo-dump-ring-task.h"
#include "ufo-ring-coordinates.h"
#include "ufo-priv.h"

struct _UfoDumpRingTaskPrivate {
    unsigned scale;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoDumpRingTask, ufo_dump_ring_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_DUMP_RING_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_DUMP_RING_TASK, UfoDumpRingTaskPrivate))

enum {
    PROP_0,
    PROP_SCALE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_dump_ring_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_DUMP_RING_TASK, NULL));
}

static void
ufo_dump_ring_task_setup (UfoTask *task,
                          UfoResources *resources,
                          GError **error)
{
}

static void
ufo_dump_ring_task_get_requisition (UfoTask *task,
                                    UfoBuffer **inputs,
                                    UfoRequisition *requisition,
                                    GError **error)
{
    ufo_buffer_get_requisition(inputs[0], requisition);
}

static guint
ufo_dump_ring_task_get_num_inputs (UfoTask *task)
{
    return 2;
}

static guint
ufo_dump_ring_task_get_num_dimensions (UfoTask *task,
                                       guint input)
{
    if (input == 0)
        /* First input is source image */
        return 2;
    else
        /* Second input is coordinates array */
        return 1;
}

static UfoTaskMode
ufo_dump_ring_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_CPU;
}

static void
copy_image_and_get_max (UfoBuffer *ufo_img, UfoBuffer *ufo_dst, float *max)
{
    float *img = ufo_buffer_get_host_array (ufo_img, NULL);
    float *dst = ufo_buffer_get_host_array (ufo_dst, NULL);
    *max = img[0];
    UfoRequisition req;
    ufo_buffer_get_requisition (ufo_img, &req);

    for (size_t y = 0; y < req.dims[1]; ++y) {
        for (size_t x = 0; x < req.dims[0]; ++x) {
            float value = img[y * req.dims[0] + x];
            dst[y * req.dims[0] + x] = value;
            *max = *max < value ? value : *max;
        }
    }
}

static void
dump_circles (UfoDumpRingTaskPrivate *priv, UfoBuffer *ufo_rings,
              UfoBuffer *ufo_dst, float max)
{
    if (max > 100)
        max += 100;
    else
        max *= 2;

    float *rings = ufo_buffer_get_host_array (ufo_rings, NULL);
    float *dst = ufo_buffer_get_host_array (ufo_dst, NULL);
    unsigned nb_rings = (unsigned) *rings;
    UfoRequisition req;
    ufo_buffer_get_requisition (ufo_dst, &req);
    UfoRingCoordinate *coord = (UfoRingCoordinate *) (rings + 1);

    for (unsigned i = 0; i < nb_rings; ++i) {
        float x = roundf (coord[i].x * (float) priv->scale);
        float y = roundf (coord[i].y * (float) priv->scale);
        float r = roundf (coord[i].r * (float) priv->scale);

        for (float t = 0; t < 2 * G_PI; t += (float) 0.005) {
            int x2 = (int) round (r * cos (t) + x);
            int y2 = (int) round (r * sin (t) + y);
            if (x2 >= 0 && x2 < (int) req.dims[0] && y2 >=0 && y2 < (int) req.dims[1]) {
                unsigned idx = (unsigned) ((unsigned) x2 + (unsigned) y2 * req.dims[0]);
                dst[idx] = max;
            }
        }
    }
}

static gboolean
ufo_dump_ring_task_process (UfoTask *task,
                            UfoBuffer **inputs,
                            UfoBuffer *output,
                            UfoRequisition *requisition)
{
    float max;
    copy_image_and_get_max (inputs[0], output, &max);
    UfoDumpRingTaskPrivate *priv = UFO_DUMP_RING_TASK_GET_PRIVATE (task);
    /* use max_value for drawing circles */
    dump_circles (priv, inputs[1], output, max);
    return TRUE;
}

static void
ufo_dump_ring_task_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
    UfoDumpRingTaskPrivate *priv = UFO_DUMP_RING_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_SCALE:
            priv->scale = g_value_get_uint(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_dump_ring_task_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
    UfoDumpRingTaskPrivate *priv = UFO_DUMP_RING_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_SCALE:
            g_value_set_uint (value, priv->scale);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_dump_ring_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_dump_ring_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_dump_ring_task_setup;
    iface->get_num_inputs = ufo_dump_ring_task_get_num_inputs;
    iface->get_num_dimensions = ufo_dump_ring_task_get_num_dimensions;
    iface->get_mode = ufo_dump_ring_task_get_mode;
    iface->get_requisition = ufo_dump_ring_task_get_requisition;
    iface->process = ufo_dump_ring_task_process;
}

static void
ufo_dump_ring_task_class_init (UfoDumpRingTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_dump_ring_task_set_property;
    gobject_class->get_property = ufo_dump_ring_task_get_property;
    gobject_class->finalize = ufo_dump_ring_task_finalize;

    properties[PROP_SCALE] =
        g_param_spec_uint ("scale",
            "Says by how much rings should be increased",
            "Says by how much rings should be increased",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoDumpRingTaskPrivate));
}

static void
ufo_dump_ring_task_init(UfoDumpRingTask *self)
{
    self->priv = UFO_DUMP_RING_TASK_GET_PRIVATE(self);
    self->priv->scale = 1;
}
