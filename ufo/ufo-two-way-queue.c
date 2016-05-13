/*
 * Copyright (C) 2011-2014 Karlsruhe Institute of Technology
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

#include <ufo/ufo-two-way-queue.h>
#include "compat.h"


struct _UfoTwoWayQueue {
    GAsyncQueue *producer_queue;
    GAsyncQueue *consumer_queue;
    GList *inserted;
    guint capacity;
};

/**
 * ufo_two_way_queue_new: (skip)
 * @init: (element-type gpointer): List with elements inserted into
 *  consumer queue
 *
 * Create a new two-way queue and optionally initialize the consumer queue with
 * elements from @init.
 *
 * Returns: A new #UfoTwoWayQueue.
 */
UfoTwoWayQueue *
ufo_two_way_queue_new (GList *init)
{
    GList *it;
    UfoTwoWayQueue *queue = g_new0 (UfoTwoWayQueue, 1);

    queue->producer_queue = g_async_queue_new ();
    queue->consumer_queue = g_async_queue_new ();
    queue->inserted = NULL;
    queue->capacity = 0;

    g_list_for (init, it) {
        ufo_two_way_queue_insert (queue, it->data);
    }

    return queue;
}

void
ufo_two_way_queue_free (UfoTwoWayQueue *queue)
{
    g_async_queue_unref (queue->producer_queue);
    g_async_queue_unref (queue->consumer_queue);
    g_list_free (queue->inserted);
    g_free (queue);
}

/**
 * ufo_two_way_queue_consumer_pop:
 * @queue: A #UfoTwoWayQueue
 *
 * Fetch an item for consumption.
 *
 * Returns: (transfer none): A consumable item.
 */
gpointer
ufo_two_way_queue_consumer_pop (UfoTwoWayQueue *queue)
{
    return g_async_queue_pop (queue->consumer_queue);
}

void
ufo_two_way_queue_consumer_push (UfoTwoWayQueue *queue, gpointer data)
{
    g_async_queue_push (queue->producer_queue, data);
}

/**
 * ufo_two_way_queue_producer_pop:
 * @queue: A #UfoTwoWayQueue
 *
 * Fetch an item for production.
 *
 * Returns: (transfer none): A producable item.
 */
gpointer
ufo_two_way_queue_producer_pop (UfoTwoWayQueue *queue)
{
    return g_async_queue_pop (queue->producer_queue);
}

void
ufo_two_way_queue_producer_push (UfoTwoWayQueue *queue, gpointer data)
{
    g_async_queue_push (queue->consumer_queue, data);
}

/**
 * ufo_two_way_queue_get_inserted:
 * @queue: A #UfoTwoWayQueue
 *
 * Fetch all items ever inserted.
 *
 * Returns: (transfer none) (element-type gpointer): A list with all items.
 */
GList *
ufo_two_way_queue_get_inserted (UfoTwoWayQueue *queue)
{
    return queue->inserted;
}

void
ufo_two_way_queue_insert (UfoTwoWayQueue *queue, gpointer data)
{
    g_async_queue_push (queue->producer_queue, data);
    queue->inserted = g_list_append (queue->inserted, data);
    queue->capacity++;
}

guint
ufo_two_way_queue_get_capacity (UfoTwoWayQueue *queue)
{
    return queue->capacity;
}
