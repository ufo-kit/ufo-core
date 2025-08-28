/*
 * Copyright (C) 2011-2017 Karlsruhe Institute of Technology
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

#ifndef __UFO_ZMQ_SUB_TASK_H
#define __UFO_ZMQ_SUB_TASK_H

#include <ufo/ufo.h>

G_BEGIN_DECLS

#define UFO_TYPE_ZMQ_SUB_TASK             (ufo_zmq_sub_task_get_type())
#define UFO_ZMQ_SUB_TASK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_ZMQ_SUB_TASK, UfoZmqSubTask))
#define UFO_IS_ZMQ_SUB_TASK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_ZMQ_SUB_TASK))
#define UFO_ZMQ_SUB_TASK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_ZMQ_SUB_TASK, UfoZmqSubTaskClass))
#define UFO_IS_ZMQ_SUB_TASK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_ZMQ_SUB_TASK))
#define UFO_ZMQ_SUB_TASK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_ZMQ_SUB_TASK, UfoZmqSubTaskClass))

typedef struct _UfoZmqSubTask           UfoZmqSubTask;
typedef struct _UfoZmqSubTaskClass      UfoZmqSubTaskClass;
typedef struct _UfoZmqSubTaskPrivate    UfoZmqSubTaskPrivate;

struct _UfoZmqSubTask {
    UfoTaskNode parent_instance;

    UfoZmqSubTaskPrivate *priv;
};

struct _UfoZmqSubTaskClass {
    UfoTaskNodeClass parent_class;
};

UfoNode  *ufo_zmq_sub_task_new       (void);
GType     ufo_zmq_sub_task_get_type  (void);

G_END_DECLS

#endif
