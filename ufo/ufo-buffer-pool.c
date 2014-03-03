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

#include <ufo/ufo-buffer-pool.h>
#include <ufo/ufo-buffer.h>

G_DEFINE_TYPE(UfoBufferPool, ufo_buffer_pool, G_TYPE_OBJECT)

#define UFO_BUFFER_POOL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BUFFER_POOL, UfoBufferPoolPrivate))

struct _UfoBufferPoolPrivate {
    GAsyncQueue *pool;
    gint allocated_buffers;
    gint capacity;
    GMutex *mutex;
    gpointer context;
};

UfoBufferPool *
ufo_buffer_pool_new (gint capacity, gpointer ocl_context)
{
    UfoBufferPool *bp;
    bp = UFO_BUFFER_POOL (g_object_new (UFO_TYPE_BUFFER_POOL, NULL));

    UfoBufferPoolPrivate *priv = UFO_BUFFER_POOL_GET_PRIVATE (bp);
    priv->context = ocl_context;

    priv->capacity = capacity;

    return bp;
}

UfoBuffer *
ufo_buffer_pool_acquire (UfoBufferPool *bp, UfoRequisition *requisition) {
    UfoBuffer *buffer;
    UfoBufferPoolPrivate *priv = UFO_BUFFER_POOL_GET_PRIVATE (bp);

    g_mutex_lock (priv->mutex);
    if (priv->allocated_buffers < priv->capacity) {
        priv->allocated_buffers++;
        g_mutex_unlock (priv->mutex);
        g_assert (requisition->n_dims == 2);
        buffer = ufo_buffer_new (requisition, bp, priv->context);
    } else {
        g_mutex_unlock (priv->mutex);
        buffer = g_async_queue_pop (priv->pool);
        if (ufo_buffer_cmp_dimensions (buffer, requisition))
            ufo_buffer_resize (buffer, requisition);
    }

    return buffer;
}

void
ufo_buffer_pool_release (UfoBufferPool *bp, UfoBuffer *buffer)
{
    g_return_if_fail (UFO_IS_BUFFER (buffer));

    UfoBufferPoolPrivate *priv = UFO_BUFFER_POOL_GET_PRIVATE (bp);
    // g_debug ("RELEASE alloc: %d\tin queue: %d", priv->allocated_buffers, g_async_queue_length (priv->pool));
    g_async_queue_push (priv->pool, buffer);
}

static void
ufo_buffer_pool_dispose (GObject *object)
{
    UfoBufferPoolPrivate *priv = UFO_BUFFER_POOL_GET_PRIVATE (object);

    // TODO try to pop the queue empty and unref every buffer

    G_OBJECT_CLASS (ufo_buffer_pool_parent_class)->dispose (object);
}

static void
ufo_buffer_pool_finalize (GObject *object)
{
    UfoBufferPoolPrivate *priv = UFO_BUFFER_POOL_GET_PRIVATE (object);
    g_mutex_free (priv->mutex);

    G_OBJECT_CLASS (ufo_buffer_pool_parent_class)->finalize (object);
}

static void
ufo_buffer_pool_class_init (UfoBufferPoolClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);
    oclass->dispose = ufo_buffer_pool_dispose;
    oclass->finalize = ufo_buffer_pool_finalize;

    g_type_class_add_private (klass, sizeof (UfoBufferPoolPrivate));
}

static void
ufo_buffer_pool_init (UfoBufferPool *self)
{
    UfoBufferPoolPrivate *priv = UFO_BUFFER_POOL_GET_PRIVATE (self);
    priv->mutex = g_mutex_new ();
    priv->pool = g_async_queue_new ();
    priv->allocated_buffers = 0;
}
