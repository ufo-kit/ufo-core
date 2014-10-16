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

#ifndef __UFO_COPY_TASK_H
#define __UFO_COPY_TASK_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <ufo/ufo-task-node.h>

G_BEGIN_DECLS

#define UFO_TYPE_COPY_TASK             (ufo_copy_task_get_type())
#define UFO_COPY_TASK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_COPY_TASK, UfoCopyTask))
#define UFO_IS_COPY_TASK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_COPY_TASK))
#define UFO_COPY_TASK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_COPY_TASK, UfoCopyTaskClass))
#define UFO_IS_COPY_TASK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_COPY_TASK))
#define UFO_COPY_TASK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_COPY_TASK, UfoCopyTaskClass))

typedef struct _UfoCopyTask           UfoCopyTask;
typedef struct _UfoCopyTaskClass      UfoCopyTaskClass;
typedef struct _UfoCopyTaskPrivate    UfoCopyTaskPrivate;

/**
 * UfoCopyTask:
 *
 * Main object for organizing filters. The contents of the #UfoCopyTask structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoCopyTask {
    /*< private >*/
    UfoTaskNode parent_instance;

    UfoCopyTaskPrivate *priv;
};

/**
 * UfoCopyTaskClass:
 *
 * #UfoCopyTask class
 */
struct _UfoCopyTaskClass {
    /*< private >*/
    UfoTaskNodeClass parent_class;
};

UfoNode   * ufo_copy_task_new                  (void);
GType       ufo_copy_task_get_type             (void);

G_END_DECLS

#endif
