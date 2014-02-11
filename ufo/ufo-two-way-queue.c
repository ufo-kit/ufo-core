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
    guint capacity;
};

UfoTwoWayQueue *
ufo_two_way_queue_new (GList *init)
{
    GList *it;
    UfoTwoWayQueue *queue = g_new0 (UfoTwoWayQueue, 1);

    queue->producer_queue = g_async_queue_new ();
    queue->consumer_queue = g_async_queue_new ();
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
    g_free (queue);
}

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

void
ufo_two_way_queue_insert (UfoTwoWayQueue *queue, gpointer data)
{
    g_async_queue_push (queue->producer_queue, data);
    queue->capacity++;
}

guint
ufo_two_way_queue_get_capacity (UfoTwoWayQueue *queue)
{
    return queue->capacity;
}
