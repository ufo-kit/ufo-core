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

#ifndef __UFO_INPUT_TASK_H
#define __UFO_INPUT_TASK_H

#include <ufo-task-node.h>

G_BEGIN_DECLS

#define UFO_TYPE_INPUT_TASK             (ufo_input_task_get_type())
#define UFO_INPUT_TASK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_INPUT_TASK, UfoInputTask))
#define UFO_IS_INPUT_TASK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_INPUT_TASK))
#define UFO_INPUT_TASK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_INPUT_TASK, UfoInputTaskClass))
#define UFO_IS_INPUT_TASK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_INPUT_TASK))
#define UFO_INPUT_TASK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_INPUT_TASK, UfoInputTaskClass))

typedef struct _UfoInputTask           UfoInputTask;
typedef struct _UfoInputTaskClass      UfoInputTaskClass;
typedef struct _UfoInputTaskPrivate    UfoInputTaskPrivate;

/**
 * UfoInputTask:
 *
 * Main object for organizing filters. The contents of the #UfoInputTask structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoInputTask {
    /*< private >*/
    UfoTaskNode parent_instance;

    UfoInputTaskPrivate *priv;
};

/**
 * UfoInputTaskClass:
 *
 * #UfoInputTask class
 */
struct _UfoInputTaskClass {
    /*< private >*/
    UfoTaskNodeClass parent_class;
};

UfoNode   * ufo_input_task_new                  (void);
void        ufo_input_task_stop                 (UfoInputTask *task);
void        ufo_input_task_release_input_buffer (UfoInputTask *task,
                                                 UfoBuffer *buffer);
UfoBuffer * ufo_input_task_get_input_buffer     (UfoInputTask *task);
GType       ufo_input_task_get_type             (void);

G_END_DECLS

#endif
