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

#ifndef __UFO_FORWARDPROJECT_TASK_H
#define __UFO_FORWARDPROJECT_TASK_H

#include <ufo/ufo.h>

G_BEGIN_DECLS

#define UFO_TYPE_FORWARDPROJECT_TASK             (ufo_forwardproject_task_get_type())
#define UFO_FORWARDPROJECT_TASK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FORWARDPROJECT_TASK, UfoForwardprojectTask))
#define UFO_IS_FORWARDPROJECT_TASK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FORWARDPROJECT_TASK))
#define UFO_FORWARDPROJECT_TASK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FORWARDPROJECT_TASK, UfoForwardprojectTaskClass))
#define UFO_IS_FORWARDPROJECT_TASK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FORWARDPROJECT_TASK))
#define UFO_FORWARDPROJECT_TASK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FORWARDPROJECT_TASK, UfoForwardprojectTaskClass))

typedef struct _UfoForwardprojectTask           UfoForwardprojectTask;
typedef struct _UfoForwardprojectTaskClass      UfoForwardprojectTaskClass;
typedef struct _UfoForwardprojectTaskPrivate    UfoForwardprojectTaskPrivate;

/**
 * UfoForwardprojectTask:
 *
 * [ADD DESCRIPTION HERE]. The contents of the #UfoForwardprojectTask structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoForwardprojectTask {
    /*< private >*/
    UfoTaskNode parent_instance;

    UfoForwardprojectTaskPrivate *priv;
};

/**
 * UfoForwardprojectTaskClass:
 *
 * #UfoForwardprojectTask class
 */
struct _UfoForwardprojectTaskClass {
    /*< private >*/
    UfoTaskNodeClass parent_class;
};

UfoNode  *ufo_forwardproject_task_new       (void);
GType     ufo_forwardproject_task_get_type  (void);

G_END_DECLS

#endif

