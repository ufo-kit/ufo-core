/**
 * SECTION:ufo-channel
 * @Short_description: Data transport between two UfoFilters
 * @Title: UfoChannel
 */

#include <string.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "config.h"
#include "ufo-resource-manager.h"
#include "ufo-channel.h"

G_DEFINE_TYPE(UfoChannel, ufo_channel, G_TYPE_OBJECT)

#define UFO_CHANNEL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CHANNEL, UfoChannelPrivate))

struct _UfoChannelPrivate {
    gint         ref_count;
    GAsyncQueue *input_queue;
    GAsyncQueue *output_queue;
    UfoChannel  *next;
};

/**
 * ufo_channel_new:
 *
 * Creates a new #UfoChannel.
 *
 * Return value: A new #UfoChannel
 */
UfoChannel *
ufo_channel_new (void)
{
    UfoChannel *channel = UFO_CHANNEL (g_object_new (UFO_TYPE_CHANNEL, NULL));
    return channel;
}

/**
 * ufo_channel_ref:
 * @channel: A #UfoChannel
 *
 * Reference a channel if to be used as an output.
 */
void
ufo_channel_ref (UfoChannel *channel)
{
    g_return_if_fail (UFO_IS_CHANNEL (channel));
    g_atomic_int_inc (&channel->priv->ref_count);
}

/**
 * ufo_channel_finish:
 * @channel: A #UfoChannel
 *
 * Finish using this channel and notify subsequent filters that no more
 * data can be expected.
 */
void
ufo_channel_finish (UfoChannel *channel)
{
    UfoChannelPrivate *priv;

    g_return_if_fail (UFO_IS_CHANNEL (channel));
    priv = UFO_CHANNEL_GET_PRIVATE (channel);

    for (gint i = 0; i < priv->ref_count; i++)
        g_async_queue_push (priv->input_queue, GINT_TO_POINTER (1));
}

/**
 * ufo_channel_finish_next:
 * @channel: A #UfoChannel
 *
 * Finish using this channel. If @channel has a daisy-chained channel, it will
 * also be finished.
 */
void
ufo_channel_finish_next (UfoChannel *channel)
{
    UfoChannelPrivate *priv;

    g_return_if_fail (UFO_IS_CHANNEL (channel));
    priv = UFO_CHANNEL_GET_PRIVATE (channel);

    ufo_channel_finish (channel);

    if (priv->next != NULL)
        ufo_channel_finish (priv->next);
}

/**
 * ufo_channel_insert:
 * @channel: A #UfoChannel
 * @buffer: A #UfoBuffer to be inserted
 *
 * Inserts an initial @buffer that can be consumed with
 * ufo_channel_fetch_output().
 */
void
ufo_channel_insert (UfoChannel *channel,
                    UfoBuffer  *buffer)
{
    g_return_if_fail (UFO_IS_CHANNEL (channel));

    if (channel->priv->next != NULL) {
        UfoChannel *next = channel->priv->next;

        while (next->priv->next != NULL)
            next = next->priv->next;

        g_async_queue_push (next->priv->output_queue, buffer);
    }
    else
        g_async_queue_push (channel->priv->output_queue, buffer);
}

/**
 * ufo_channel_fetch_input: (skip)
 * @channel: A #UfoChannel
 *
 * Get a new buffer from the channel that can be consumed as an input.  This
 * method blocks execution as long as no new input buffer the available from the
 * preceding filter. Use ufo_channel_release_input() to return the buffer to the
 * channel.
 *
 * Return value: The next #UfoBuffer input
 */
UfoBuffer *
ufo_channel_fetch_input (UfoChannel *channel)
{
    g_return_val_if_fail (UFO_IS_CHANNEL (channel), NULL);
    return g_async_queue_pop (channel->priv->input_queue);
}

/**
 * ufo_channel_release_input:
 * @channel: A #UfoChannel
 * @buffer: A #UfoBuffer acquired with ufo_channel_fetch_input()
 *
 * Release a buffer that was acquired with ufo_channel_fetch_input().
 */
void
ufo_channel_release_input (UfoChannel *channel, UfoBuffer *buffer)
{
    g_return_if_fail (UFO_IS_CHANNEL (channel));

    if (channel->priv->next != NULL)
        g_async_queue_push (channel->priv->next->priv->input_queue, buffer);
    else
        g_async_queue_push (channel->priv->output_queue, buffer);
}

/**
 * ufo_channel_fetch_output: (skip)
 * @channel: A #UfoChannel
 *
 * Get a new buffer from the channel that can be consumed as an output.  This
 * method blocks execution as long as no new input buffer the available from the
 * successing filter. Use ufo_channel_release_output() to return the buffer to
 * the channel.
 *
 * Return value: The next #UfoBuffer for output
 */
UfoBuffer *
ufo_channel_fetch_output (UfoChannel *channel)
{
    UfoChannel *next;

    g_return_val_if_fail (UFO_IS_CHANNEL (channel), NULL);

    next = channel->priv->next;

    if (next == NULL) {
        return g_async_queue_pop (channel->priv->output_queue);
    }
    else {
        while (next->priv->next != NULL)
            next = next->priv->next;

        return g_async_queue_pop (next->priv->output_queue);
    }
}

/**
 * ufo_channel_release_output:
 * @channel: A #UfoChannel
 * @buffer: A #UfoBuffer acquired with ufo_channel_fetch_output()
 *
 * Release a buffer that was acquired with ufo_channel_fetch_output().
 */
void
ufo_channel_release_output (UfoChannel *channel, UfoBuffer *buffer)
{
    g_return_if_fail (UFO_IS_CHANNEL (channel) && UFO_IS_BUFFER (buffer));
    g_async_queue_push (channel->priv->input_queue, buffer);
}

void ufo_channel_daisy_chain (UfoChannel *channel,
                              UfoChannel *next)
{
    g_return_if_fail (UFO_IS_CHANNEL (channel) && UFO_IS_CHANNEL (next));
    channel->priv->next = next;
}

static void
ufo_channel_dispose (GObject *object)
{
    G_OBJECT_CLASS (ufo_channel_parent_class)->dispose (object);
}

static void
ufo_channel_finalize (GObject *object)
{
    UfoChannelPrivate *priv = UFO_CHANNEL_GET_PRIVATE (object);

    g_async_queue_unref (priv->input_queue);
    g_async_queue_unref (priv->output_queue);

    G_OBJECT_CLASS (ufo_channel_parent_class)->finalize (object);
}

static void
ufo_channel_class_init (UfoChannelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->dispose = ufo_channel_dispose;
    gobject_class->finalize = ufo_channel_finalize;
    g_type_class_add_private (klass, sizeof (UfoChannelPrivate));
}

static void
ufo_channel_init (UfoChannel *channel)
{
    UfoChannelPrivate *priv;
    channel->priv = priv = UFO_CHANNEL_GET_PRIVATE (channel);

    priv->ref_count = 0;
    priv->input_queue = g_async_queue_new ();
    priv->output_queue = g_async_queue_new ();
    priv->next = NULL;
}
