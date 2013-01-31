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

#ifndef __UFO_DUMMY_TASK_H
#define __UFO_DUMMY_TASK_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <ufo/ufo-task-node.h>

G_BEGIN_DECLS

#define UFO_TYPE_DUMMY_TASK             (ufo_dummy_task_get_type())
#define UFO_DUMMY_TASK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_DUMMY_TASK, UfoDummyTask))
#define UFO_IS_DUMMY_TASK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_DUMMY_TASK))
#define UFO_DUMMY_TASK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_DUMMY_TASK, UfoDummyTaskClass))
#define UFO_IS_DUMMY_TASK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_DUMMY_TASK))
#define UFO_DUMMY_TASK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_DUMMY_TASK, UfoDummyTaskClass))

typedef struct _UfoDummyTask           UfoDummyTask;
typedef struct _UfoDummyTaskClass      UfoDummyTaskClass;
typedef struct _UfoDummyTaskPrivate    UfoDummyTaskPrivate;

/**
 * UfoDummyTask:
 *
 * Main object for organizing filters. The contents of the #UfoDummyTask structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoDummyTask {
    /*< private >*/
    UfoTaskNode parent_instance;

    UfoDummyTaskPrivate *priv;
};

/**
 * UfoDummyTaskClass:
 *
 * #UfoDummyTask class
 */
struct _UfoDummyTaskClass {
    /*< private >*/
    UfoTaskNodeClass parent_class;
};

UfoNode   * ufo_dummy_task_new      (void);
GType       ufo_dummy_task_get_type (void);

G_END_DECLS

#endif
