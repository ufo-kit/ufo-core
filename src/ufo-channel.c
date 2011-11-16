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
    gboolean finished;
    GAsyncQueue *queue;

    UfoBuffer *buffers[2]; /* For now, just double buffering */
    guint current_output;
    GAsyncQueue *input_queue;
};


/* 
 * Public Interface
 */

/**
 * \brief Create a new channel
 * \public \memberof UfoChannel
 *
 * \return UfoChannel object
 */
UfoChannel *ufo_channel_new(void)
{
    UfoChannel *channel = UFO_CHANNEL(g_object_new(UFO_TYPE_CHANNEL, NULL));
    return channel;
}

/**
 * \brief Reference a channel if to be used as an output
 * \public \memberof UfoChannel
 *
 * \param[in] channel Channel to be referenced
 */
void ufo_channel_ref(UfoChannel *channel)
{
    g_atomic_int_inc(&channel->priv->ref_count);
}

/**
 * \brief Finish using this channel and notify subsequent filters that no more
 * data can be expected
 * \public \memberof UfoChannel
 *
 * \param[in] channel Channel to be finished
 */
void ufo_channel_finish(UfoChannel *channel)
{
    channel->priv->finished = g_atomic_int_dec_and_test(&channel->priv->ref_count);
}

/**
 * \brief Query number of currently placed buffers in this buffer
 * \public \memberof UfoChannel
 *
 * \param[in] UfoChannel
 */
gint ufo_channel_length(UfoChannel *channel)
{
    return g_async_queue_length(channel->priv->queue);
}

/**
 * \brief Pop data from the channel
 * \public \memberof UfoChannel
 *
 * \return A UfoBuffer object or NULL if no more data is going to be put into
 * the queue.
 */
UfoBuffer *ufo_channel_pop(UfoChannel *channel)
{
    UfoBuffer *buffer = NULL;
    UfoChannelPrivate *priv = UFO_CHANNEL_GET_PRIVATE(channel);
    GTimeVal end_time;
    
    while (!priv->finished || (g_async_queue_length(priv->queue) > 0)) {
        g_get_current_time(&end_time);
        g_time_val_add(&end_time, 10000);
        buffer = g_async_queue_timed_pop(priv->queue, &end_time);
        if (buffer != NULL)
            return buffer;
    }
    
    return buffer;
}

/**
 * \brief Push data into a channel
 * \public \memberof UfoChannel
 *
 * \param[in] channel UfoChannel to push data into
 * \param[in] buffer UfoBuffer to be pushed into channel
 */
void ufo_channel_push(UfoChannel *channel, UfoBuffer *buffer)
{
    UfoChannelPrivate *priv = UFO_CHANNEL_GET_PRIVATE(channel);
    while (g_async_queue_length(priv->queue) > 4)
        g_usleep(10000); /* sleep for 10ms */

    g_async_queue_push(channel->priv->queue, buffer);
}

/* 
 * Virtual Methods 
 */
static void ufo_channel_finalize(GObject *gobject)
{
    UfoChannel *channel = UFO_CHANNEL(gobject);
    g_async_queue_unref(channel->priv->queue);
    g_async_queue_unref(channel->priv->input_queue);
    G_OBJECT_CLASS(ufo_channel_parent_class)->finalize(gobject);
}

/**
 * \note Not thread-safe
 */
void ufo_channel_allocate_output_buffers(UfoChannel *channel, guint width, guint height)
{
    UfoChannelPrivate *priv = UFO_CHANNEL_GET_PRIVATE(channel);
    UfoResourceManager *manager = ufo_resource_manager();
    gint32 dimensions[4] = { width, height, 1, 1 };
    priv->buffers[0] = ufo_resource_manager_request_buffer(manager, UFO_BUFFER_2D, dimensions, NULL, NULL);
    priv->buffers[1] = ufo_resource_manager_request_buffer(manager, UFO_BUFFER_2D, dimensions, NULL, NULL);
}

UfoBuffer *ufo_channel_get_input_buffer(UfoChannel *channel)
{
    UfoChannelPrivate *priv = UFO_CHANNEL_GET_PRIVATE(channel);
    if (priv->finished)
        return NULL;
    return g_async_queue_pop(priv->input_queue);
}

UfoBuffer *ufo_channel_get_output_buffer(UfoChannel *channel)
{
    UfoChannelPrivate *priv = UFO_CHANNEL_GET_PRIVATE(channel);
    UfoBuffer *buffer = priv->buffers[priv->current_output];
    priv->current_output = 1 - priv->current_output;
    return buffer;
}

void ufo_channel_finalize_output_buffer(UfoChannel *channel, UfoBuffer *buffer)
{
    UfoChannelPrivate *priv = UFO_CHANNEL_GET_PRIVATE(channel);
    g_async_queue_push(priv->input_queue, buffer);
}


/*
 * Type/Class Initialization
 */
static void ufo_channel_class_init(UfoChannelClass *klass)
{
    /* override methods */
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = ufo_channel_finalize;

    /* install private data */
    g_type_class_add_private(klass, sizeof(UfoChannelPrivate));
}

static void ufo_channel_init(UfoChannel *channel)
{
    UfoChannelPrivate *priv;
    channel->priv = priv = UFO_CHANNEL_GET_PRIVATE(channel);
    priv->queue = g_async_queue_new();
    priv->ref_count = 0;
    priv->finished = FALSE;
    priv->buffers[0] = priv->buffers[1] = NULL;
    priv->current_output = 0;
    priv->input_queue = g_async_queue_new();
}
