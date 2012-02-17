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

G_DEFINE_TYPE(UfoChannel, ufo_channel, G_TYPE_OBJECT);

#define UFO_CHANNEL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CHANNEL, UfoChannelPrivate))

struct _UfoChannelPrivate {
    gint ref_count;
    gint ref_total;
    gboolean finished;

    guint num_buffers;
    UfoBuffer **buffers;
    GAsyncQueue *input_queue;
    GAsyncQueue *output_queue;
};

static void channel_dispose_buffers(UfoChannelPrivate *priv)
{
    UfoResourceManager *manager = ufo_resource_manager();

    g_async_queue_unref(priv->input_queue);
    g_async_queue_unref(priv->output_queue);
    priv->input_queue = g_async_queue_new();
    priv->output_queue = g_async_queue_new();

    for (int i = 0; i < priv->num_buffers; i++)
        ufo_resource_manager_release_buffer(manager, priv->buffers[i]);

    priv->num_buffers = 0;
}

/**
 * ufo_channel_new:
 *
 * Creates a new #UfoChannel.
 *
 * Return value: A new #UfoChannel
 */
UfoChannel *ufo_channel_new(void)
{
    UfoChannel *channel = UFO_CHANNEL(g_object_new(UFO_TYPE_CHANNEL, NULL));
    return channel;
}

/**
 * ufo_channel_ref:
 * @channel: A #UfoChannel
 *
 * Reference a channel if to be used as an output.
 */
void ufo_channel_ref(UfoChannel *channel)
{
    g_return_if_fail(UFO_IS_CHANNEL(channel));
    g_atomic_int_inc(&channel->priv->ref_count);
    g_atomic_int_inc(&channel->priv->ref_total);
}

/**
 * ufo_channel_finish:
 * @channel: A #UfoChannel
 *
 * Finish using this channel and notify subsequent filters that no more
 * data can be expected.
 */
void ufo_channel_finish(UfoChannel *channel)
{
    g_return_if_fail(UFO_IS_CHANNEL(channel));
    UfoChannelPrivate *priv = UFO_CHANNEL_GET_PRIVATE(channel);
    priv->finished = g_atomic_int_dec_and_test(&priv->ref_count);

    /* All buffers are released and queues setup clean again. This is necessary
     * so that on subsequent runs everything is in a clean state and prepared
     * for ufo_channel_allocate_output_buffers().
     *
     * Note that the test itself is thread-safe because the last thread was the
     * only able to toggle the finished flag. */
    if (priv->finished) {

        /* We have to block here, because we do not know how long the subsequent
         * filters need to process the remaining inputs */
        while (g_async_queue_length(priv->input_queue) != 0)
            g_usleep(1000);

        while (g_async_queue_length(priv->output_queue) != priv->num_buffers)
            g_usleep(1000);

        /* All inputs must be consumed ... */
        g_assert(g_async_queue_length(priv->input_queue) == 0);

        /* ... and be returned */
        g_assert(g_async_queue_length(priv->output_queue) == priv->num_buffers);

        channel_dispose_buffers(priv);
        priv->ref_count = priv->ref_total;
        priv->finished = FALSE;
        g_free(priv->buffers);
        priv->buffers = NULL;
    }
}

/**
 * ufo_channel_allocate_output_buffers:
 * @channel: A #UfoChannel
 * @num_dims: (in): Number of dimensions
 * @dim_size: (in) (array): Size of the buffers
 *
 * Allocate outgoing buffers with @num_dims dimensions. @num_dims must be less than
 * or equal to #UFO_BUFFER_MAX_NDIMS.
 */
void ufo_channel_allocate_output_buffers(UfoChannel *channel, guint num_dims, const guint *dim_size)
{
    g_return_if_fail(UFO_IS_CHANNEL(channel));
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
    g_static_mutex_lock(&mutex);
    UfoChannelPrivate *priv = UFO_CHANNEL_GET_PRIVATE(channel);

    if (priv->buffers != NULL) {
        /* Buffers are already allocated by another thread, so leave here */
        g_static_mutex_unlock(&mutex);
        return;
    }

    UfoResourceManager *manager = ufo_resource_manager();
    /* Allocate as many buffers as we have threads */
    priv->num_buffers = priv->ref_count;
    priv->buffers = g_malloc0(priv->num_buffers * sizeof(UfoBuffer *));

    for (int i = 0; i < priv->num_buffers; i++) {
        priv->buffers[i] = ufo_resource_manager_request_buffer(manager, num_dims, dim_size, NULL, NULL);
        g_async_queue_push(priv->output_queue, priv->buffers[i]);
    }

    g_static_mutex_unlock(&mutex);
}

/**
 * ufo_channel_allocate_output_buffers_like:
 * @channel: A #UfoChannel
 * @buffer: A #UfoBuffer whose dimensions should be used for the output buffers
 *
 * Allocate outgoing buffers with dimensions given by @buffer.
 */
void ufo_channel_allocate_output_buffers_like(UfoChannel *channel, UfoBuffer *buffer)
{
    g_return_if_fail(UFO_IS_CHANNEL(channel) || UFO_IS_BUFFER(buffer));
    guint num_dims = 0;
    guint *dim_size = NULL;
    ufo_buffer_get_dimensions(buffer, &num_dims, &dim_size);
    ufo_channel_allocate_output_buffers(channel, num_dims, dim_size);
    g_free(dim_size);
}

/**
 * ufo_channel_get_input_buffer:
 * @channel: A #UfoChannel
 *
 * This method blocks execution as long as no new input buffer is readily
 * processed by the preceding filter.
 *
 * Return value: The next #UfoBuffer input
 */
UfoBuffer *ufo_channel_get_input_buffer(UfoChannel *channel)
{
    g_return_val_if_fail(UFO_IS_CHANNEL(channel), NULL);
    UfoChannelPrivate *priv = UFO_CHANNEL_GET_PRIVATE(channel);
    UfoBuffer *buffer = NULL;
    GTimeVal end_time;

    while ((!priv->finished) || (g_async_queue_length(priv->input_queue) > 0)) {
        g_get_current_time(&end_time);
        g_time_val_add(&end_time, 1000);
        buffer = g_async_queue_timed_pop(priv->input_queue, &end_time);

        if (buffer != NULL)
            return buffer;
    }

    return buffer;
}

/**
 * ufo_channel_get_output_buffer:
 * @channel: A #UfoChannel
 *
 * This method blocks execution as long as no new output buffer is readily
 * processed by the subsequent filter.
 *
 * Return value: The next #UfoBuffer for output
 */
UfoBuffer *ufo_channel_get_output_buffer(UfoChannel *channel)
{
    g_return_val_if_fail(UFO_IS_CHANNEL(channel), NULL);
    UfoChannelPrivate *priv = UFO_CHANNEL_GET_PRIVATE(channel);
    return g_async_queue_pop(priv->output_queue);
}

/**
 * ufo_channel_finalize_input_buffer:
 * @channel: A #UfoChannel
 * @buffer: The #UfoBuffer input acquired with ufo_channel_get_input_buffer()
 *
 * An input buffer is owned by a filter by calling
 * ufo_channel_get_input_buffer() and has to be released again with this method,
 * so that a preceding filter can use it again as an output.
 */
void ufo_channel_finalize_input_buffer(UfoChannel *channel, UfoBuffer *buffer)
{
    g_return_if_fail(UFO_IS_CHANNEL(channel));
    UfoChannelPrivate *priv = UFO_CHANNEL_GET_PRIVATE(channel);

    for (int i = 0; i < priv->num_buffers; i++)
        g_assert(buffer == priv->buffers[i]);

    g_async_queue_push(priv->output_queue, buffer);
}

/**
 * ufo_channel_finalize_output_buffer:
 * @channel: A #UfoChannel
 * @buffer: The #UfoBuffer input acquired with ufo_channel_get_output_buffer()
 *
 * An output buffer is owned by a filter by calling
 * ufo_channel_get_output_buffer() and has to be released again with this method,
 * so that a subsequent filter can use it as an input.
 */
void ufo_channel_finalize_output_buffer(UfoChannel *channel, UfoBuffer *buffer)
{
    g_return_if_fail(UFO_IS_CHANNEL(channel) || UFO_IS_BUFFER(buffer));
    UfoChannelPrivate *priv = UFO_CHANNEL_GET_PRIVATE(channel);

    for (int i = 0; i < priv->num_buffers; i++)
        g_assert(buffer == priv->buffers[i]);

    g_async_queue_push(priv->input_queue, buffer);
}

static void ufo_channel_dispose(GObject *object)
{
    UfoChannel *channel = UFO_CHANNEL(object);
    UfoChannelPrivate *priv = UFO_CHANNEL_GET_PRIVATE(channel);

    if (priv->num_buffers > 0)
        channel_dispose_buffers(priv);
}

static void ufo_channel_finalize(GObject *object)
{
    UfoChannel *channel = UFO_CHANNEL(object);
    UfoChannelPrivate *priv = UFO_CHANNEL_GET_PRIVATE(channel);
    g_free(priv->buffers);
    g_async_queue_unref(priv->input_queue);
    g_async_queue_unref(priv->output_queue);
    G_OBJECT_CLASS(ufo_channel_parent_class)->finalize(object);
}

static void ufo_channel_class_init(UfoChannelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = ufo_channel_dispose;
    gobject_class->finalize = ufo_channel_finalize;
    g_type_class_add_private(klass, sizeof(UfoChannelPrivate));
}

static void ufo_channel_init(UfoChannel *channel)
{
    UfoChannelPrivate *priv;
    channel->priv = priv = UFO_CHANNEL_GET_PRIVATE(channel);
    priv->ref_count = 0;
    priv->ref_total = 0;
    priv->finished = FALSE;
    priv->num_buffers = 0;
    priv->buffers = NULL;
    priv->input_queue = g_async_queue_new();
    priv->output_queue = g_async_queue_new();
}
