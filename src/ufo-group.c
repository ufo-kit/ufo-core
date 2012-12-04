#include <CL/cl.h>
#include "ufo-group.h"
#include "ufo-task-node.h"

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

/**
 * ufo_group_new:
 * @targets: (element-type UfoNode): A list of #UfoNode targets
 * @context: A cl_context on which the targets should operate on.
 *
 * Returns: A new #UfoGroup.
 */
UfoGroup *
ufo_group_new (GList *targets,
               gpointer context)
{
    UfoGroup *group;
    UfoGroupPrivate *priv;

    group = UFO_GROUP (g_object_new (UFO_TYPE_GROUP, NULL));
    priv = group->priv;

    priv->targets = targets;
    priv->n_targets = g_list_length (targets);
    priv->queues = g_new0 (UfoQueue *, priv->n_targets);
    priv->pattern = UFO_SEND_SCATTER;
    priv->current = 0;
    priv->context = context;

    for (guint i = 0; i < priv->n_targets; i++)
        priv->queues[i] = ufo_queue_new ();

    return group;
}

UfoBuffer *
ufo_group_pop_output_buffer (UfoGroup *group,
                             UfoRequisition *requisition)
{
    UfoBuffer *output;
    UfoGroupPrivate *priv;

    priv = group->priv;

    if (ufo_queue_get_capacity (priv->queues[priv->current]) < priv->n_targets) {
        output = ufo_buffer_new (requisition, priv->context);
        priv->buffers = g_list_append (priv->buffers, output);
        ufo_queue_insert (priv->queues[priv->current],
                          UFO_QUEUE_PRODUCER,
                          output);
    }

    output = ufo_queue_pop (priv->queues[priv->current],
                            UFO_QUEUE_PRODUCER);

    if (!ufo_buffer_cmp_dimensions (output, requisition))
        ufo_buffer_resize (output, requisition);

    return output;
}

void
ufo_group_push_output_buffer (UfoGroup *group,
                              UfoBuffer *buffer)
{
    UfoGroupPrivate *priv;

    priv = group->priv;

    /* Copy or not depending on the send pattern */
    if (priv->pattern == UFO_SEND_SCATTER) {
        ufo_queue_push (priv->queues[priv->current],
                        UFO_QUEUE_PRODUCER,
                        buffer);

        priv->current = (priv->current + 1) % priv->n_targets;
    }
    else
        g_print ("Other send methods are not yet supported\n");
}

UfoBuffer *
ufo_group_pop_input_buffer (UfoGroup *group,
                            UfoTask *target)
{
    UfoGroupPrivate *priv;
    UfoBuffer *input;
    gint pos;

    priv = group->priv;
    pos = g_list_index (priv->targets, target);
    input = pos >= 0 ? ufo_queue_pop (priv->queues[pos], UFO_QUEUE_CONSUMER) : NULL;

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

    if (pos >= 0)
        ufo_queue_push (priv->queues[pos], UFO_QUEUE_CONSUMER, input);
}

void
ufo_group_finish (UfoGroup *group)
{
    UfoGroupPrivate *priv;

    priv = group->priv;

    for (guint i = 0; i < priv->n_targets; i++) {
        ufo_queue_push (priv->queues[i],
                        UFO_QUEUE_PRODUCER,
                        UFO_END_OF_STREAM);
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
    g_list_free (priv->buffers);
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
