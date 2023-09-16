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

#ifndef __UFO_REPLICATE_TASK_H
#define __UFO_REPLICATE_TASK_H

#include <ufo/ufo.h>

G_BEGIN_DECLS

#define UFO_TYPE_REPLICATE_TASK             (ufo_replicate_task_get_type())
#define UFO_REPLICATE_TASK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_REPLICATE_TASK, UfoReplicateTask))
#define UFO_IS_REPLICATE_TASK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_REPLICATE_TASK))
#define UFO_REPLICATE_TASK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_REPLICATE_TASK, UfoReplicateTaskClass))
#define UFO_IS_REPLICATE_TASK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_REPLICATE_TASK))
#define UFO_REPLICATE_TASK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_REPLICATE_TASK, UfoReplicateTaskClass))

typedef struct _UfoReplicateTask           UfoReplicateTask;
typedef struct _UfoReplicateTaskClass      UfoReplicateTaskClass;
typedef struct _UfoReplicateTaskPrivate    UfoReplicateTaskPrivate;

/**
 * UfoReplicateTask:
 *
 * [ADD DESCRIPTION HERE]. The contents of the #UfoReplicateTask structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoReplicateTask {
    /*< private >*/
    UfoTaskNode parent_instance;

    UfoReplicateTaskPrivate *priv;
};

/**
 * UfoReplicateTaskClass:
 *
 * #UfoReplicateTask class
 */
struct _UfoReplicateTaskClass {
    /*< private >*/
    UfoTaskNodeClass parent_class;
};

UfoNode  *ufo_replicate_task_new       (void);
GType     ufo_replicate_task_get_type  (void);

G_END_DECLS

#endif

