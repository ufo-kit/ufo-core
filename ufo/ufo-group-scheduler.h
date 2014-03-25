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

#ifndef __UFO_GROUP_SCHEDULER_H
#define __UFO_GROUP_SCHEDULER_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <ufo/ufo-config.h>
#include <ufo/ufo-task-graph.h>
#include <ufo/ufo-base-scheduler.h>

G_BEGIN_DECLS

#define UFO_TYPE_GROUP_SCHEDULER             (ufo_group_scheduler_get_type())
#define UFO_GROUP_SCHEDULER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_GROUP_SCHEDULER, UfoGroupScheduler))
#define UFO_IS_GROUP_SCHEDULER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_GROUP_SCHEDULER))
#define UFO_GROUP_SCHEDULER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_GROUP_SCHEDULER, UfoGroupSchedulerClass))
#define UFO_IS_GROUP_SCHEDULER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_GROUP_SCHEDULER))
#define UFO_GROUP_SCHEDULER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_GROUP_SCHEDULER, UfoGroupSchedulerClass))

#define UFO_GROUP_SCHEDULER_ERROR            ufo_group_scheduler_error_quark()

typedef struct _UfoGroupScheduler           UfoGroupScheduler;
typedef struct _UfoGroupSchedulerClass      UfoGroupSchedulerClass;
typedef struct _UfoGroupSchedulerPrivate    UfoGroupSchedulerPrivate;

typedef enum {
    UFO_GROUP_SCHEDULER_ERROR_SETUP
} UfoGroupSchedulerError;

/**
 * UfoGroupScheduler:
 *
 * The base class scheduler is responsible of assigning command queues to
 * filters (thus managing GPU device resources) and decide if to run a GPU or a
 * CPU. The actual schedule planning can be overriden.
 */
struct _UfoGroupScheduler {
    /*< private >*/
    UfoBaseScheduler parent_instance;

    UfoGroupSchedulerPrivate *priv;
};

/**
 * UfoGroupSchedulerClass:
 *
 * #UfoGroupScheduler class
 */
struct _UfoGroupSchedulerClass {
    /*< private >*/
    UfoBaseSchedulerClass parent_class;
};

UfoBaseScheduler    *ufo_group_scheduler_new            (UfoConfig  *config);
GType                ufo_group_scheduler_get_type       (void);
GQuark               ufo_group_scheduler_error_quark    (void);

G_END_DECLS

#endif
