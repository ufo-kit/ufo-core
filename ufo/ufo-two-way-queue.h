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

#ifndef __UFO_TWO_WAY_QUEUE_H
#define __UFO_TWO_WAY_QUEUE_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

typedef struct _UfoTwoWayQueue          UfoTwoWayQueue;

UfoTwoWayQueue  * ufo_two_way_queue_new             (GList *init);
void              ufo_two_way_queue_free            (UfoTwoWayQueue *queue);
gpointer          ufo_two_way_queue_consumer_pop    (UfoTwoWayQueue *queue);
void              ufo_two_way_queue_consumer_push   (UfoTwoWayQueue *queue,
                                                     gpointer data);
gpointer          ufo_two_way_queue_producer_pop    (UfoTwoWayQueue *queue);
void              ufo_two_way_queue_producer_push   (UfoTwoWayQueue *queue,
                                                     gpointer data);
void              ufo_two_way_queue_insert          (UfoTwoWayQueue *queue,
                                                     gpointer data);
guint             ufo_two_way_queue_get_capacity    (UfoTwoWayQueue *queue);

G_END_DECLS

#endif
