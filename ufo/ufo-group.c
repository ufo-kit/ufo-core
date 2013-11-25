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

G_DEFINE_TYPE (UfoGroup, ufo_group, G_TYPE_OBJECT)

#define UFO_GROUP_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GROUP, UfoGroupPrivate))

typedef struct {
    GAsyncQueue *queues[2];
    guint        capacity;
} UfoQueue;

struct _UfoGroupPrivate {
    GList           *targets;
    guint            n_targets;
    UfoQueue       **queues;
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

typedef enum {
    UFO_QUEUE_PRODUCER = 0,
    UFO_QUEUE_CONSUMER = 1
} UfoQueueAccess;

static UfoQueue     *ufo_queue_new          (void);
static gpointer      ufo_queue_pop          (UfoQueue *queue, UfoQueueAccess access);
static void          ufo_queue_push         (UfoQueue *queue, UfoQueueAccess access, gpointer data);
static void          ufo_queue_insert       (UfoQueue *queue, UfoQueueAccess access, gpointer data);
static guint         ufo_queue_get_capacity (UfoQueue *queue);
static guint         ufo_queue_length       (UfoQueue *queue, UfoQueueAccess access);

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

static GTimer *global_clock;
typedef struct {
    gdouble start;
    gchar *msg;
    UfoQueue *queue;
    gint pos;
    UfoQueueAccess access;
} QueueTrace;

static inline gpointer start_trace_queue (const gchar *msg, UfoQueue *queue, gint pos, UfoQueueAccess access)
{
    QueueTrace *h = g_malloc0(sizeof(QueueTrace));
    h->start = g_timer_elapsed (global_clock, NULL);
    h->queue = queue;
    h->pos = pos;
    h->msg = g_strdup (msg);
    h->access = access;
    return (gpointer) h;
}
static inline void stop_trace_queue (gpointer data)
{
    QueueTrace *h = (QueueTrace *) data;
    gdouble now = g_timer_elapsed (global_clock, NULL);
    gdouble delta = now - h->start;
    gchar *a;
    if (h->access == UFO_QUEUE_PRODUCER)
        a = g_strdup ("UFO_QUEUE_PRODUCER");
    else
        a = g_strdup ("UFO_QUEUE_CONSUMER");
    g_debug ("[%.8f] [%s] [%s] [%p]", delta, h->msg, a, (void*) h->queue);

    g_free (h->msg);
    g_free (h);
}

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
    priv->queues = g_new0 (UfoQueue *, priv->n_targets);
    priv->n_expected = g_new0 (gint, priv->n_targets);
    priv->pattern = pattern;
    priv->current = 0;
    priv->context = context;
    priv->n_received = 0;

    for (guint i = 0; i < priv->n_targets; i++)
        priv->queues[i] = ufo_queue_new ();

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

    if (ufo_queue_get_capacity (priv->queues[pos]) < (priv->n_targets + 1)) {
        buffer = ufo_buffer_new (requisition, priv->context);
        priv->buffers = g_list_append (priv->buffers, buffer);
        gpointer h = start_trace_queue ("INSERT", priv->queues[pos], pos, UFO_QUEUE_PRODUCER);
        ufo_queue_insert (priv->queues[pos], UFO_QUEUE_PRODUCER, buffer);
        stop_trace_queue (h);
    }

    gpointer h = start_trace_queue ("POP", priv->queues[pos], pos, UFO_QUEUE_PRODUCER);
    buffer = ufo_queue_pop (priv->queues[pos], UFO_QUEUE_PRODUCER);
    stop_trace_queue (h);

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
    UfoGroupPrivate *priv = group->priv;
    UfoBuffer *buffer = ufo_buffer_new (requisition, priv->context);
    priv->buffers = g_list_append (priv->buffers, buffer);

    return buffer;
    // UfoGroupPrivate *priv;
    // guint pos = 0;

    // priv = group->priv;

    // if ((priv->pattern == UFO_SEND_SCATTER) || (priv->pattern == UFO_SEND_SEQUENTIAL)) {
    //     g_debug ("pop output buffer - my pos is %d", pos);
    //     pos = priv->current;
    // }

    // return pop_or_alloc_buffer (priv, pos, requisition);
}

static void push_buffer_roundrobin (UfoGroup *group, UfoBuffer *buffer)
{
    UfoGroupPrivate *priv = group->priv;
    gpointer h = start_trace_queue ("PUSH", priv->queues[priv->current], priv->current, UFO_QUEUE_PRODUCER);
    ufo_queue_push (priv->queues[priv->current],
        UFO_QUEUE_PRODUCER,
        buffer);
    stop_trace_queue (h);
    priv->current = (priv->current + 1) % priv->n_targets;
    g_debug("priv->current: %d, priv->n_targets: %d", priv->current, priv->n_targets);
}

static void push_buffer_least_utilized (UfoGroup *group, UfoBuffer *buffer)
{
    UfoGroupPrivate *priv = group->priv;
    // find a queue with least current utilization
    guint lowest_len = G_MAXUINT;
    guint selected;
    for (guint i = 0; i < priv->n_targets; i++) {
        guint len = ufo_queue_length (priv->queues[i], UFO_QUEUE_PRODUCER);
        if (len < lowest_len) {
            lowest_len = len;
            selected = i;
        }
    }
    g_debug ("selected queue with lowest utilization: %d", selected);
    ufo_queue_push (priv->queues[selected], UFO_QUEUE_PRODUCER, buffer);
}

void
ufo_group_push_output_buffer (UfoGroup *group,
                              UfoBuffer *buffer)
{
    UfoGroupPrivate *priv;

    priv = group->priv;
    priv->n_received++;

    g_debug("n_received: %d", priv->n_received);
    /* Copy or not depending on the send pattern */
    if (priv->pattern == UFO_SEND_SCATTER) {
        push_buffer_roundrobin (group, buffer);
        // push_buffer_least_utilized (group, buffer);
    }
    return;
    // else if (priv->pattern == UFO_SEND_BROADCAST) {
    //     g_debug("SEND_BROADCAST");
    //     g_assert (FALSE);
    //     UfoRequisition requisition;

    //     ufo_buffer_get_requisition (buffer, &requisition);

    //     for (guint pos = 1; pos < priv->n_targets; pos++) {
    //         UfoBuffer *copy;

    //         copy = pop_or_alloc_buffer (priv, pos, &requisition);
    //         ufo_buffer_copy (buffer, copy);
    //         ufo_queue_push (priv->queues[pos], UFO_QUEUE_PRODUCER, copy);
    //     }

    //     ufo_queue_push (priv->queues[0], UFO_QUEUE_PRODUCER, buffer);
    // }
    // else if (priv->pattern == UFO_SEND_SEQUENTIAL) {
    //     g_assert (FALSE);
    //     g_debug("SEND_SEQUENTIAL");
    //     ufo_queue_push (priv->queues[priv->current],
    //                     UFO_QUEUE_PRODUCER,
    //                     buffer);

    //     if (priv->n_expected[priv->current] == priv->n_received) {
    //         ufo_queue_push (priv->queues[priv->current],
    //                         UFO_QUEUE_PRODUCER,
    //                         UFO_END_OF_STREAM);
    //         /* FIXME: setting priv->current to 0 again wouldn't be right */
    //         priv->current = (priv->current + 1) % priv->n_targets;
    //         priv->n_received = 0;
    //     }
    // }
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
    gpointer h = start_trace_queue ("POP", priv->queues[pos], pos, UFO_QUEUE_CONSUMER);
    input = pos >= 0 ? ufo_queue_pop (priv->queues[pos], UFO_QUEUE_CONSUMER) : NULL;
    stop_trace_queue (h);
    return input;
}

void
ufo_group_push_input_buffer (UfoGroup *group,
                             UfoTask *target,
                             UfoBuffer *input)
{
    UfoGroupPrivate *priv;
    gint pos;

    priv = group->priv;
    pos = g_list_index (priv->targets, target);

    if (pos >= 0) {
        gpointer h = start_trace_queue ("PUSH", priv->queues[pos], pos, UFO_QUEUE_CONSUMER);
        ufo_queue_push (priv->queues[pos], UFO_QUEUE_CONSUMER, input);
        stop_trace_queue (h);
    }
}

void
ufo_group_finish (UfoGroup *group)
{
    UfoGroupPrivate *priv;

    priv = group->priv;

    for (guint i = 0; i < priv->n_targets; i++) {
        gpointer h = start_trace_queue ("PUSH EOS", priv->queues[i], i, UFO_QUEUE_PRODUCER);
        ufo_queue_push (priv->queues[i],
                        UFO_QUEUE_PRODUCER,
                        UFO_END_OF_STREAM);
        stop_trace_queue (h);
    }
}

static UfoQueue *
ufo_queue_new (void)
{
    UfoQueue *queue = g_new0 (UfoQueue, 1);
    queue->queues[0] = g_async_queue_new ();
    queue->queues[1] = g_async_queue_new ();
    queue->capacity  = 0;
    return queue;
}

static void
ufo_queue_free (UfoQueue *queue)
{
    g_async_queue_unref (queue->queues[0]);
    g_async_queue_unref (queue->queues[1]);
    g_free (queue);
}

static guint
ufo_queue_length (UfoQueue *queue, UfoQueueAccess access)
{
    return g_async_queue_length (queue->queues[access]);
}

static gpointer
ufo_queue_pop (UfoQueue *queue, UfoQueueAccess access)
{
    return g_async_queue_pop (queue->queues[access]);
}

static void
ufo_queue_push (UfoQueue *queue, UfoQueueAccess access, gpointer data)
{
    g_async_queue_push (queue->queues[1-access], data);
}

static void
ufo_queue_insert (UfoQueue *queue, UfoQueueAccess access, gpointer data)
{
    g_async_queue_push (queue->queues[access], data);
    queue->capacity++;
}

static guint
ufo_queue_get_capacity (UfoQueue *queue)
{
    return queue->capacity;
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
        ufo_queue_free (priv->queues[i]);

    g_free (priv->queues);
    priv->queues = NULL;

    G_OBJECT_CLASS (ufo_group_parent_class)->finalize (object);
}

static void
ufo_group_class_init (UfoGroupClass *klass)
{
    if (global_clock == NULL)
        global_clock = g_timer_new();

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
