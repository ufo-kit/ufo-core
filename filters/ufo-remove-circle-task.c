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
#include "ufo-remove-circle-task.h"
#include "ufo-ring-coordinates.h"


struct _UfoRemoveCircleTaskPrivate {
    float threshold;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoRemoveCircleTask, ufo_remove_circle_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_REMOVE_CIRCLE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_REMOVE_CIRCLE_TASK, UfoRemoveCircleTaskPrivate))

enum {
    PROP_0,
    PROP_THRESHOLD,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_remove_circle_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_REMOVE_CIRCLE_TASK, NULL));
}

static void
ufo_remove_circle_task_setup (UfoTask *task,
                       UfoResources *resources,
                       GError **error)
{
}

static void
ufo_remove_circle_task_get_requisition (UfoTask *task,
                                        UfoBuffer **inputs,
                                        UfoRequisition *requisition,
                                        GError **error)
{
    /* At most we will have same sized buffer as input since we are only
     * removing circles  */
    ufo_buffer_get_requisition(inputs[0], requisition);
}

static guint
ufo_remove_circle_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_remove_circle_task_get_num_dimensions (UfoTask *task,
                                           guint input)
{
    return 1;
}

static UfoTaskMode
ufo_remove_circle_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_CPU;
}

static unsigned
is_intersection(UfoRemoveCircleTaskPrivate *priv, UfoRingCoordinate *left,
                UfoRingCoordinate *center, UfoRingCoordinate *right)
{
    float threshold = priv->threshold;
    float lr_dx = left->x - right->x;
    float lr_dy = left->y - right->y;
    float lr_dist = sqrtf(lr_dx * lr_dx + lr_dy * lr_dy);

    /* We want left and right to intersect */
    if (left->r + right->r <= lr_dist)
        return 0;

    UfoRingCoordinate lr_mid = {
        .x = (left->x + right->x) / 2,
        .y = (left->y + right->y) / 2,
        .r = (left->r + right->r) / 2
    };
    float r_inner = (left->r + right->r - lr_dist) / 2;
    float r_outer = (left->r + right->r + lr_dist) / 2;

    float dr_in = fabsf(center->r - r_inner);
    float dr_out = fabsf(center->r - r_outer);
    /* Center radi must have a raddi approximately equal to the inner or outer
     * ring radius */
    if (dr_out >= threshold && dr_in >= threshold)
        return 0;

    /* Compute distance from center to the midpoint location of left and right */
    float cm_dx = center->x - lr_mid.x;
    float cm_dy = center->y - lr_mid.y;
    float cm_dist = sqrtf(cm_dx * cm_dx + cm_dy * cm_dy);
    /* Center has to be close enough to the optimal intersection center point */
    if (cm_dist >= lr_dist / 2)
        return 0;

    return 1;
}

static void remove_circle(UfoRemoveCircleTaskPrivate *priv, URCS *src, URCS *dst)
{
    unsigned nb_elt = (unsigned) src->nb_elt;
    /* Number of elements to keep */
    unsigned res_counter = 0;
    /* Check if any of the elements is actually the intersection of two others
     * rings */
    for (unsigned center = 0; center < nb_elt; ++center) {
        /* Flag to check if center is an intersecting ring */
        unsigned is_intersecting_ring = 0;
        /* Check if there exists a  pair (not containing center) of rings such
         * as center rings is indeed the intersection of that pair of rings */
        for (unsigned left = 0; left < nb_elt; ++left) {
            if (left == center)
                continue;

            for (unsigned right = left + 1; right < nb_elt; ++right) {
                if (right == center)
                    continue;

                if (is_intersection(priv, &src->coord[left], &src->coord[center], &src->coord[right])) {
                    is_intersecting_ring = 1;
                    break;
                }
            }

            /* Stop searching if we know that centers needs removing */
            if (is_intersecting_ring)
                break;
        }

        if (!is_intersecting_ring) {
            dst->coord[res_counter] = src->coord[center];
            ++res_counter;
        }
    }
    dst->nb_elt = (float) res_counter;
}

static
int get_min_contrast(URCS *src)
{
    int min_i = 0;
    char found = 0;
    for (int i = 0; i < src->nb_elt; ++i){
        if (src->coord[i].contrast) {
            found = 1;
            if (src->coord[min_i].contrast * src->coord[min_i].intensity >
                src->coord[i].contrast * src->coord[i].intensity ||
                src->coord[min_i].contrast == 0)
                min_i = i;
        }
    }
    if (!found)
        return -1;
    return min_i;
}

static
void remove_inner_circle(URCS *src, URCS *dst)
{
    int min_i = get_min_contrast(src);
    unsigned counter = 0;

    while (min_i != -1 && counter < src->nb_elt) {
        for (int i = 0; i < src->nb_elt; ++i) {
            if (!src->coord[i].contrast || i == min_i)
                continue;

            float dx = src->coord[i].x - src->coord[min_i].x;
            float dy = src->coord[i].y - src->coord[min_i].y;
            float dist = sqrtf(dx * dx + dy * dy);
            /* Check if other ring is touching current ring */
            float big_rad = src->coord[min_i].r;
            float small_rad = src->coord[i].r;

            if (small_rad > big_rad) {
                small_rad = big_rad;
                big_rad = src->coord[i].r;
            }

            if (fabs(dist + small_rad - big_rad) < 6) {
                /* Check if other ring contrast is less to current ring */
                /* If so remove it, as it likely is a false positive, i-e it was
                 * using part of current ring */
                if ((src->coord[i].contrast * src->coord[i].intensity) /
                    (src->coord[min_i].contrast * src->coord[min_i].intensity) < 0.75)
                    src->coord[i].contrast = 0;
            }
        }
        dst->coord[counter] = src->coord[min_i];
        ++counter;
        /* Notify that we have check this ring to not process it again */
        src->coord[min_i].contrast = 0;
        min_i = get_min_contrast(src);
    }
    dst->nb_elt = (float) counter;
    if (counter >= src->nb_elt && min_i != -1)
        g_print("Remove Circle: Some memory corruption occurred. This is a bug "
                " in the software that needs fixing\n");
}

static gboolean
ufo_remove_circle_task_process (UfoTask *task,
                                UfoBuffer **inputs,
                                UfoBuffer *output,
                                UfoRequisition *requisition)
{
    UfoBuffer* duplicate = ufo_buffer_dup (inputs[0]);
    ufo_buffer_copy (inputs[0], duplicate);
    URCS *coord = (URCS *) ufo_buffer_get_host_array (duplicate, NULL);
    URCS *out_coord = (URCS *) ufo_buffer_get_host_array (output, NULL);
    UfoRemoveCircleTaskPrivate *priv = UFO_REMOVE_CIRCLE_TASK_GET_PRIVATE(task);
    remove_inner_circle (coord, out_coord);

    ufo_buffer_copy (output, duplicate);
    remove_circle (priv, coord, out_coord);

    g_object_unref (duplicate);
    return TRUE;
}

static void
ufo_remove_circle_task_set_property (GObject *object,
                                     guint property_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
    UfoRemoveCircleTaskPrivate *priv = UFO_REMOVE_CIRCLE_TASK_GET_PRIVATE (object);

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
ufo_remove_circle_task_get_property (GObject *object,
                                     guint property_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
    UfoRemoveCircleTaskPrivate *priv = UFO_REMOVE_CIRCLE_TASK_GET_PRIVATE (object);

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
ufo_remove_circle_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_remove_circle_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_remove_circle_task_setup;
    iface->get_num_inputs = ufo_remove_circle_task_get_num_inputs;
    iface->get_num_dimensions = ufo_remove_circle_task_get_num_dimensions;
    iface->get_mode = ufo_remove_circle_task_get_mode;
    iface->get_requisition = ufo_remove_circle_task_get_requisition;
    iface->process = ufo_remove_circle_task_process;
}

static void
ufo_remove_circle_task_class_init (UfoRemoveCircleTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_remove_circle_task_set_property;
    gobject_class->get_property = ufo_remove_circle_task_get_property;
    gobject_class->finalize = ufo_remove_circle_task_finalize;

    properties[PROP_THRESHOLD] =
        g_param_spec_float ("threshold",
            "Set maximum Inner and outer ring radii size difference",
            "Set maximum inner and outer ring radii size difference",
            1, G_MAXFLOAT, 5,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoRemoveCircleTaskPrivate));
}

static void
ufo_remove_circle_task_init(UfoRemoveCircleTask *self)
{
    self->priv = UFO_REMOVE_CIRCLE_TASK_GET_PRIVATE(self);
    self->priv->threshold = 5;
}
