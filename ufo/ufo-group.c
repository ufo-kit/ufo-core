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

#include <CL/cl.h>
#include <ufo/ufo-group.h>
#include <ufo/ufo-task-node.h>
#include <ufo/ufo-two-way-queue.h>

G_DEFINE_TYPE (UfoGroup, ufo_group, G_TYPE_OBJECT)

#define UFO_GROUP_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GROUP, UfoGroupPrivate))

struct _UfoGroupPrivate {
    GList           *targets;
    guint            n_targets;
    UfoTwoWayQueue  **queues;
    gint            *n_expected;
    gint             n_received;
    gboolean        *ready;
    UfoSendPattern   pattern;
    guint            current;
    cl_context       context;
    GList           *buffers;
};

enum {
    PROP_0,
    N_PROPERTIES
};


/**
 * ufo_group_new:
 * @targets: (element-type UfoNode): A list of #UfoNode targets
 * @context: A cl_context on which the targets should operate on.
 * @pattern: Pattern to distribute data among the @targets
 *
 * Create a new #UfoGroup.
 *
 * Returns: A new #UfoGroup.
 */
UfoGroup *
ufo_group_new (GList *targets,
               gpointer context,
               UfoSendPattern pattern)
{
    UfoGroup *group;
    UfoGroupPrivate *priv;

    group = UFO_GROUP (g_object_new (UFO_TYPE_GROUP, NULL));
    priv = group->priv;

    priv->targets = g_list_copy (targets);
    priv->n_targets = g_list_length (targets);
    priv->queues = g_new0 (UfoTwoWayQueue *, priv->n_targets);
    priv->n_expected = g_new0 (gint, priv->n_targets);
    priv->pattern = pattern;
    priv->current = 0;
    priv->context = context;
    priv->n_received = 0;

    for (guint i = 0; i < priv->n_targets; i++)
        priv->queues[i] = ufo_two_way_queue_new (NULL);

    return group;
}

guint
ufo_group_get_num_targets (UfoGroup *group)
{
    g_return_val_if_fail (UFO_IS_GROUP (group), 0);
    return group->priv->n_targets;
}

static UfoBuffer *
pop_or_alloc_buffer (UfoGroupPrivate *priv,
                     guint pos,
                     UfoRequisition *requisition)
{
    UfoBuffer *buffer;

    if (ufo_two_way_queue_get_capacity (priv->queues[pos]) < (priv->n_targets + 1)) {
        buffer = ufo_buffer_new (requisition, NULL, priv->context);
        priv->buffers = g_list_append (priv->buffers, buffer);
        ufo_two_way_queue_insert (priv->queues[pos], buffer);
    }

    buffer = ufo_two_way_queue_producer_pop (priv->queues[pos]);

    if (ufo_buffer_cmp_dimensions (buffer, requisition))
        ufo_buffer_resize (buffer, requisition);

    return buffer;
}

/**
 * ufo_group_pop_output_buffer:
 * @group: A #UfoGroup
 * @requisition: Size of the buffer.
 *
 * Return value: (transfer full): A newly allocated buffer or a re-used buffer
 * that must be released with ufo_group_push_output_buffer().
 */
UfoBuffer *
ufo_group_pop_output_buffer (UfoGroup *group,
                             UfoRequisition *requisition)
{
    UfoGroupPrivate *priv;
    guint pos = 0;

    priv = group->priv;

    if ((priv->pattern == UFO_SEND_SCATTER) || (priv->pattern == UFO_SEND_SEQUENTIAL))
        pos = priv->current;

    return pop_or_alloc_buffer (priv, pos, requisition);
}

void
ufo_group_push_output_buffer (UfoGroup *group,
                              UfoBuffer *buffer)
{
    UfoGroupPrivate *priv;

    priv = group->priv;
    priv->n_received++;

    if (buffer == NULL) {
        g_critical ("buffer was NULL!");
    }
    /* Copy or not depending on the send pattern */
    if (priv->pattern == UFO_SEND_SCATTER) {
        ufo_two_way_queue_producer_push (priv->queues[priv->current], buffer);
        priv->current = (priv->current + 1) % priv->n_targets;
    }
    else if (priv->pattern == UFO_SEND_BROADCAST) {
        UfoRequisition requisition;

        ufo_buffer_get_requisition (buffer, &requisition);

        for (guint pos = 1; pos < priv->n_targets; pos++) {
            UfoBuffer *copy;

            copy = pop_or_alloc_buffer (priv, pos, &requisition);
            ufo_buffer_copy (buffer, copy);
            ufo_two_way_queue_producer_push (priv->queues[pos], copy);
        }

        ufo_two_way_queue_producer_push (priv->queues[0], buffer);
    }
    else if (priv->pattern == UFO_SEND_SEQUENTIAL) {
        ufo_two_way_queue_producer_push (priv->queues[priv->current], buffer);

        if (priv->n_expected[priv->current] == priv->n_received) {
            ufo_two_way_queue_producer_push (priv->queues[priv->current], UFO_END_OF_STREAM);

            /* FIXME: setting priv->current to 0 again wouldn't be right */
            priv->current = (priv->current + 1) % priv->n_targets;
            priv->n_received = 0;
        }
    }
}

void
ufo_group_set_num_expected (UfoGroup *group,
                            UfoTask *target,
                            gint n_expected)
{
    UfoGroupPrivate *priv;
    gint pos;

    g_return_if_fail (UFO_IS_GROUP (group));
    priv = group->priv;
    pos = g_list_index (priv->targets, target);
    priv->n_expected[pos] = n_expected;
}

/**
 * ufo_group_pop_input_buffer:
 * @group: A #UfoGroup
 * @target: The #UfoTask that is a target in @group
 *
 * Return value: (transfer full): A buffer that must be released with
 * ufo_group_push_input_buffer().
 */
UfoBuffer *
ufo_group_pop_input_buffer (UfoGroup *group,
                            UfoTask *target)
{
    UfoGroupPrivate *priv;
    UfoBuffer *input;
    gint pos;

    priv = group->priv;
    pos = g_list_index (priv->targets, target);
    input = pos >= 0 ? ufo_two_way_queue_consumer_pop (priv->queues[pos]) : NULL;

    return input;
}

void
ufo_group_push_input_buffer (UfoGroup *group,
                             UfoTask *target,
                             UfoBuffer *input)
{
    UfoGroupPrivate *priv;
    gint pos;

    if (group == NULL) {
        // G_BREAKPOINT();
    }
    priv = group->priv;
    pos = g_list_index (priv->targets, target);

    if (pos >= 0)
        ufo_two_way_queue_consumer_push (priv->queues[pos], input);
}

void
ufo_group_finish (UfoGroup *group)
{
    UfoGroupPrivate *priv;

    priv = group->priv;

    for (guint i = 0; i < priv->n_targets; i++)
        ufo_two_way_queue_producer_push (priv->queues[i], UFO_END_OF_STREAM);
}

static void
ufo_group_dispose(GObject *object)
{
    UfoGroupPrivate *priv;

    priv = UFO_GROUP_GET_PRIVATE (object);
    g_list_foreach (priv->buffers, (GFunc) g_object_unref, NULL);
    G_OBJECT_CLASS (ufo_group_parent_class)->dispose (object);
}

static void
ufo_group_finalize (GObject *object)
{
    UfoGroupPrivate *priv;

    priv = UFO_GROUP_GET_PRIVATE (object);

    g_list_free (priv->targets);
    priv->targets = NULL;

    g_list_free (priv->buffers);
    priv->buffers = NULL;

    for (guint i = 0; i < priv->n_targets; i++)
        ufo_two_way_queue_free (priv->queues[i]);

    g_free (priv->queues);
    priv->queues = NULL;

    G_OBJECT_CLASS (ufo_group_parent_class)->finalize (object);
}

static void
ufo_group_class_init (UfoGroupClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose  = ufo_group_dispose;
    gobject_class->finalize = ufo_group_finalize;

    g_type_class_add_private(klass, sizeof(UfoGroupPrivate));
}

static void
ufo_group_init (UfoGroup *self)
{
    UfoGroupPrivate *priv;
    self->priv = priv = UFO_GROUP_GET_PRIVATE (self);
    priv->buffers = NULL;
}
