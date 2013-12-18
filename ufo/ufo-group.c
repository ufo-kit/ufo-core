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
#include <ufo/ufo-remote-node.h>

G_DEFINE_TYPE (UfoGroup, ufo_group, G_TYPE_OBJECT)

#define UFO_GROUP_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GROUP, UfoGroupPrivate))

typedef struct {
    GAsyncQueue *queues[2];
    guint        capacity;
} UfoQueue;

struct _UfoGroupPrivate {
    guint            max_queue_capacity;
    UfoSendPattern   pattern;
    GList           *targets;
    guint            n_received;
    UfoQueue        *queue;
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
static void          ufo_queue_push         (UfoQueue *queue, UfoQueueAccess access, gpointer data, UfoTask *target);
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
    guint length_before;
} QueueTrace;

static inline gpointer start_trace_queue (const gchar *msg, UfoQueue *queue, UfoGroupPrivate *priv, UfoQueueAccess access)
{
    return NULL;
    QueueTrace *h = g_malloc0(sizeof(QueueTrace));
    h->start = g_timer_elapsed (global_clock, NULL);
    h->queue = queue;
    h->msg = g_strdup (msg);
    h->access = access;
    h->length_before = ufo_queue_length (queue, access);
    return (gpointer) h;
}

static inline void stop_trace_queue (gpointer data)
{
    return;
    QueueTrace *h = (QueueTrace *) data;
    gdouble now = g_timer_elapsed (global_clock, NULL);
    gdouble delta = now - h->start;
    gchar *a;
    if (h->access == UFO_QUEUE_PRODUCER)
        a = g_strdup ("UFO_QUEUE_PRODUCER");
    else
        a = g_strdup ("UFO_QUEUE_CONSUMER");

    g_debug ("%.4f [%p] %.4f %s %s %p", h->start, (void*) g_thread_self(), delta, h->msg, a, (void*) h->queue);

    g_free (h->msg);
    g_free (h);
}

static gboolean
is_remote_node (UfoNode *node, gpointer user_data)
{
    const gchar *name = ufo_task_node_get_plugin_name (UFO_TASK_NODE (node));
    if (name == NULL)
        return TRUE;
    return FALSE;
}

static gboolean
is_writer_node (UfoNode *node, gpointer user_data)
{
    const gchar *name = ufo_task_node_get_plugin_name (UFO_TASK_NODE (node));
    if (name == NULL)
        return FALSE;
    return g_str_has_prefix (name, "writer");
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
    priv->queue = g_new0 (UfoQueue, 1);
    priv->pattern = pattern;
    priv->context = context;
    priv->n_received = 0;

    priv->queue = ufo_queue_new ();

    return group;
}

guint
ufo_group_get_num_targets (UfoGroup *group)
{
    g_return_val_if_fail (UFO_IS_GROUP (group), 0);
    return g_list_length (group->priv->targets);
}

GList *
ufo_group_get_targets (UfoGroup *group)
{
    return g_list_copy (group->priv->targets);
}

static UfoBuffer *
pop_or_alloc_buffer (UfoGroupPrivate *priv, UfoRequisition *requisition)
{
    UfoBuffer *buffer;
    if (ufo_queue_get_capacity (priv->queue) < priv->max_queue_capacity) {
        buffer = ufo_buffer_new (requisition, NULL, priv->context);
        priv->buffers = g_list_append (priv->buffers, buffer);
        ufo_queue_insert (priv->queue, UFO_QUEUE_PRODUCER, buffer);
    }
    buffer = ufo_queue_pop (priv->queue, UFO_QUEUE_PRODUCER);

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

    // if ((priv->pattern == UFO_SEND_SCATTER) || (priv->pattern == UFO_SEND_SEQUENTIAL)) {
    //     g_debug ("pop output buffer - my pos is %d", pos);
    // }

    return pop_or_alloc_buffer (priv, requisition);
}

static void push_buffer_roundrobin (UfoGroup *group, UfoBuffer *buffer, UfoTask *target)
{
    UfoGroupPrivate *priv = group->priv;
    ufo_queue_push (priv->queue, UFO_QUEUE_PRODUCER, buffer, target);
}

void
ufo_group_push_output_buffer (UfoGroup *group,
                              UfoBuffer *buffer)
{
    UfoGroupPrivate *priv;

    priv = group->priv;
    priv->n_received++;

    // g_debug("n_received: %d", priv->n_received);
    /* Copy or not depending on the send pattern */
    if (priv->pattern == UFO_SEND_SCATTER) {
        push_buffer_roundrobin (group, buffer, NULL);
        // push_buffer_least_utilized (group, buffer);
    }
    else if (priv->pattern == UFO_SEND_BROADCAST) {
        // TODO needs implementing
        g_debug("SEND_BROADCAST");
        g_assert (FALSE);
    }
    else if (priv->pattern == UFO_SEND_SEQUENTIAL) {
        // TODO needs implementing
        g_assert (FALSE);
    }
}

void
ufo_group_set_num_expected (UfoGroup *group,
                            UfoTask *target,
                            gint n_expected)
{
    //TODO remove
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
    UfoGroupPrivate *priv = group->priv;
    UfoBuffer *input;

    input = ufo_queue_pop (priv->queue, UFO_QUEUE_CONSUMER);
    return input;
}

void
ufo_group_push_input_buffer (UfoGroup *group,
                             UfoTask *target,
                             UfoBuffer *input)
{
    // TODO remove target from signature
    UfoGroupPrivate *priv;
    priv = group->priv;
    ufo_queue_push (priv->queue, UFO_QUEUE_CONSUMER, input, target);
}

void
ufo_group_finish (UfoGroup *group)
{
    UfoGroupPrivate *priv;
    g_debug("GROUP FINSISH");
    priv = group->priv;

    // TODO we don't know how many groups are pop-ing from us, make it enough
    for (int i=0; i<= 16; i++)
        ufo_queue_push (priv->queue, UFO_QUEUE_PRODUCER, UFO_END_OF_STREAM, NULL);
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
    gpointer h = start_trace_queue ("ufo_queue_pop", queue, NULL, 1);
    gpointer el = g_async_queue_pop (queue->queues[access]);
    stop_trace_queue (h);
    return el;
}

static void
ufo_queue_push (UfoQueue *queue, UfoQueueAccess access, gpointer data, UfoTask *target)
{
    gchar *msg;
    if (target != NULL) {
        UfoTaskNode *node = UFO_TASK_NODE (target);
        gchar *unique = ufo_task_node_get_unique_name (node);
        msg = g_strdup_printf ("ufo_queue_push TARGET: %s", unique);
    } else
        msg = g_strdup ("ufo_queue_push");

    gpointer h = start_trace_queue (msg, queue, NULL, 1-access);
    g_async_queue_push (queue->queues[1-access], data);
    stop_trace_queue (h);

}

static void
ufo_queue_insert (UfoQueue *queue, UfoQueueAccess access, gpointer data)
{
    gpointer h = start_trace_queue ("ufo_queue INSERT CAP++", queue, NULL, access);
    g_async_queue_push (queue->queues[access], data);
    stop_trace_queue (h);
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

    ufo_queue_free (priv->queue);
    g_free (priv->queue);
    priv->queue = NULL;

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
    priv->max_queue_capacity = 4;
}
