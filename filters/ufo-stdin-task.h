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

#ifndef __UFO_STDIN_TASK_H
#define __UFO_STDIN_TASK_H

#include <ufo/ufo.h>

G_BEGIN_DECLS

#define UFO_TYPE_STDIN_TASK             (ufo_stdin_task_get_type())
#define UFO_STDIN_TASK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_STDIN_TASK, UfoStdinTask))
#define UFO_IS_STDIN_TASK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_STDIN_TASK))
#define UFO_STDIN_TASK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_STDIN_TASK, UfoStdinTaskClass))
#define UFO_IS_STDIN_TASK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_STDIN_TASK))
#define UFO_STDIN_TASK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_STDIN_TASK, UfoStdinTaskClass))

typedef struct _UfoStdinTask           UfoStdinTask;
typedef struct _UfoStdinTaskClass      UfoStdinTaskClass;
typedef struct _UfoStdinTaskPrivate    UfoStdinTaskPrivate;

struct _UfoStdinTask {
    UfoTaskNode parent_instance;

    UfoStdinTaskPrivate *priv;
};

struct _UfoStdinTaskClass {
    UfoTaskNodeClass parent_class;
};

UfoNode  *ufo_stdin_task_new       (void);
GType     ufo_stdin_task_get_type  (void);

G_END_DECLS

#endif

