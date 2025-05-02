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

#ifndef __UFO_SLIDING_STACK_TASK_H
#define __UFO_SLIDING_STACK_TASK_H

#include <ufo/ufo.h>

G_BEGIN_DECLS

#define UFO_TYPE_SLIDING_STACK_TASK             (ufo_sliding_stack_task_get_type())
#define UFO_SLIDING_STACK_TASK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_SLIDING_STACK_TASK, UfoSlidingStackTask))
#define UFO_IS_SLIDING_STACK_TASK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_SLIDING_STACK_TASK))
#define UFO_SLIDING_STACK_TASK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_SLIDING_STACK_TASK, UfoSlidingStackTaskClass))
#define UFO_IS_SLIDING_STACK_TASK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_SLIDING_STACK_TASK))
#define UFO_SLIDING_STACK_TASK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_SLIDING_STACK_TASK, UfoSlidingStackTaskClass))

typedef struct _UfoSlidingStackTask           UfoSlidingStackTask;
typedef struct _UfoSlidingStackTaskClass      UfoSlidingStackTaskClass;
typedef struct _UfoSlidingStackTaskPrivate    UfoSlidingStackTaskPrivate;

struct _UfoSlidingStackTask {
    UfoTaskNode parent_instance;

    UfoSlidingStackTaskPrivate *priv;
};

struct _UfoSlidingStackTaskClass {
    UfoTaskNodeClass parent_class;
};

UfoNode  *ufo_sliding_stack_task_new       (void);
GType     ufo_sliding_stack_task_get_type  (void);

G_END_DECLS

#endif
