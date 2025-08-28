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

#ifndef __UFO_ZMQ_PUB_TASK_H
#define __UFO_ZMQ_PUB_TASK_H

#include <ufo/ufo.h>

G_BEGIN_DECLS

#define UFO_TYPE_ZMQ_PUB_TASK             (ufo_zmq_pub_task_get_type())
#define UFO_ZMQ_PUB_TASK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_ZMQ_PUB_TASK, UfoZmqPubTask))
#define UFO_IS_ZMQ_PUB_TASK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_ZMQ_PUB_TASK))
#define UFO_ZMQ_PUB_TASK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_ZMQ_PUB_TASK, UfoZmqPubTaskClass))
#define UFO_IS_ZMQ_PUB_TASK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_ZMQ_PUB_TASK))
#define UFO_ZMQ_PUB_TASK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_ZMQ_PUB_TASK, UfoZmqPubTaskClass))

typedef struct _UfoZmqPubTask           UfoZmqPubTask;
typedef struct _UfoZmqPubTaskClass      UfoZmqPubTaskClass;
typedef struct _UfoZmqPubTaskPrivate    UfoZmqPubTaskPrivate;

struct _UfoZmqPubTask {
    UfoTaskNode parent_instance;

    UfoZmqPubTaskPrivate *priv;
};

struct _UfoZmqPubTaskClass {
    UfoTaskNodeClass parent_class;
};

UfoNode  *ufo_zmq_pub_task_new       (void);
GType     ufo_zmq_pub_task_get_type  (void);

G_END_DECLS

#endif
