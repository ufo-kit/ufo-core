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

#include "ufo-center-of-rotation-task.h"

/**
 * SECTION:ufo-center-of-rotation-task
 * @Short_description: Compute the center of rotation
 * @Title: center_of_rotation
 *
 */

struct _UfoCenterOfRotationTaskPrivate {
    gdouble angle_step;
    gdouble center;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoCenterOfRotationTask, ufo_center_of_rotation_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_CENTER_OF_ROTATION_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CENTER_OF_ROTATION_TASK, UfoCenterOfRotationTaskPrivate))

enum {
    PROP_0,
    PROP_ANGLE_STEP,
    PROP_CENTER,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_center_of_rotation_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_CENTER_OF_ROTATION_TASK, NULL));
}

static void
ufo_center_of_rotation_task_setup (UfoTask *task,
                                   UfoResources *resources,
                                   GError **error)
{
}

static void
ufo_center_of_rotation_task_get_requisition (UfoTask *task,
                                             UfoBuffer **inputs,
                                             UfoRequisition *requisition,
                                             GError **error)
{
    requisition->n_dims = 0;
}

static guint
ufo_center_of_rotation_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_center_of_rotation_task_get_num_dimensions (UfoTask *task, guint input)
{
    g_return_val_if_fail (input == 0, 0);
    return 2;
}

static UfoTaskMode
ufo_center_of_rotation_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_CPU;
}

static guint
minimum_index (gfloat *data, guint n_elements)
{
    guint index;
    gfloat min;

    index = 0;
    min = data[index];

    for (guint i = 1; i < n_elements; i++) {
        if (data[i] < min) {
            index = i;
            min = data[i];
        }
    }

    return index;
}

static gboolean
ufo_center_of_rotation_task_process (UfoTask *task,
                                     UfoBuffer **inputs,
                                     UfoBuffer *output,
                                     UfoRequisition *requisition)
{
    UfoCenterOfRotationTaskPrivate *priv;
    UfoRequisition in_req;
    guint width;
    guint height;
    gfloat *proj_0;
    gfloat *proj_180;
    guint max_displacement;
    guint N;
    guint score_index;
    gfloat *scores;

    priv = UFO_CENTER_OF_ROTATION_TASK_GET_PRIVATE (task);

    ufo_buffer_get_requisition (inputs[0], &in_req);
    width = (guint) in_req.dims[0];
    height = (guint) in_req.dims[1];

    proj_0 = ufo_buffer_get_host_array (inputs[0], NULL);
    proj_180 = proj_0 + (height - 1) * width;

    max_displacement = width / 2;
    N = max_displacement * 2 - 1;
    scores = g_malloc0 (N * sizeof(gfloat));

    for (gint displacement = (-((gint) max_displacement) + 1); displacement < 0; displacement++) {
        const guint index = (guint) displacement + max_displacement - 1;
        const guint max_x = width - ((guint) ABS (displacement));

        for (guint x = 0; x < max_x; x++) {
            gfloat diff = proj_0[x] - proj_180[(max_x - x + 1)];
            scores[index] += diff * diff;
        }
    }

    for (guint displacement = 0; displacement < max_displacement; displacement++) {
        const guint index = displacement + max_displacement - 1;

        for (guint x = 0; x < width-displacement; x++) {
            gfloat diff = proj_0[x+displacement] - proj_180[(width-x+1)];
            scores[index] += diff * diff;
        }
    }

    score_index = minimum_index (scores, N);
    priv->center = (width + score_index - max_displacement + 1) / 2.0;
    g_object_notify_by_pspec (G_OBJECT (task), properties[PROP_CENTER]);
    g_free (scores);
    return TRUE;
}

static void
ufo_center_of_rotation_task_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    UfoCenterOfRotationTaskPrivate *priv = UFO_CENTER_OF_ROTATION_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_ANGLE_STEP:
            priv->angle_step = g_value_get_double (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_center_of_rotation_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoCenterOfRotationTaskPrivate *priv = UFO_CENTER_OF_ROTATION_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_ANGLE_STEP:
            g_value_set_double (value, priv->angle_step);
            break;
        case PROP_CENTER:
            g_value_set_double (value, priv->center);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_center_of_rotation_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_center_of_rotation_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_center_of_rotation_task_setup;
    iface->get_num_inputs = ufo_center_of_rotation_task_get_num_inputs;
    iface->get_num_dimensions = ufo_center_of_rotation_task_get_num_dimensions;
    iface->get_mode = ufo_center_of_rotation_task_get_mode;
    iface->get_requisition = ufo_center_of_rotation_task_get_requisition;
    iface->process = ufo_center_of_rotation_task_process;
}

static void
ufo_center_of_rotation_task_class_init (UfoCenterOfRotationTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_center_of_rotation_task_set_property;
    gobject_class->get_property = ufo_center_of_rotation_task_get_property;
    gobject_class->finalize = ufo_center_of_rotation_task_finalize;

    properties[PROP_ANGLE_STEP] =
        g_param_spec_double("angle-step",
            "Step between two successive projections",
            "Step between two successive projections",
            0.00001f, 180.0f, 1.0f,
            G_PARAM_READWRITE);

    properties[PROP_CENTER] =
        g_param_spec_double("center",
            "Center of rotation",
            "The calculated center of rotation",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
            G_PARAM_READABLE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoCenterOfRotationTaskPrivate));
}

static void
ufo_center_of_rotation_task_init(UfoCenterOfRotationTask *self)
{
    self->priv = UFO_CENTER_OF_ROTATION_TASK_GET_PRIVATE(self);
    self->priv->angle_step = G_PI / 180.0;
    self->priv->center = 0.0;
}
