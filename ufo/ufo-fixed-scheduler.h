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

#ifndef __UFO_FIXED_SCHEDULER_H
#define __UFO_FIXED_SCHEDULER_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <ufo/ufo-config.h>
#include <ufo/ufo-task-graph.h>
#include <ufo/ufo-base-scheduler.h>

G_BEGIN_DECLS

#define UFO_TYPE_FIXED_SCHEDULER             (ufo_fixed_scheduler_get_type())
#define UFO_FIXED_SCHEDULER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FIXED_SCHEDULER, UfoFixedScheduler))
#define UFO_IS_FIXED_SCHEDULER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FIXED_SCHEDULER))
#define UFO_FIXED_SCHEDULER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FIXED_SCHEDULER, UfoFixedSchedulerClass))
#define UFO_IS_FIXED_SCHEDULER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FIXED_SCHEDULER))
#define UFO_FIXED_SCHEDULER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FIXED_SCHEDULER, UfoFixedSchedulerClass))

#define UFO_FIXED_SCHEDULER_ERROR            ufo_fixed_scheduler_error_quark()

typedef struct _UfoFixedScheduler           UfoFixedScheduler;
typedef struct _UfoFixedSchedulerClass      UfoFixedSchedulerClass;
typedef struct _UfoFixedSchedulerPrivate    UfoFixedSchedulerPrivate;

typedef enum {
    UFO_FIXED_SCHEDULER_ERROR_SETUP
} UfoFixedSchedulerError;

/**
 * UfoFixedScheduler:
 *
 * The base class scheduler is responsible of assigning command queues to
 * filters (thus managing GPU device resources) and decide if to run a GPU or a
 * CPU. The actual schedule planning can be overriden.
 */
struct _UfoFixedScheduler {
    /*< private >*/
    UfoBaseScheduler parent_instance;

    UfoFixedSchedulerPrivate *priv;
};

/**
 * UfoFixedSchedulerClass:
 *
 * #UfoFixedScheduler class
 */
struct _UfoFixedSchedulerClass {
    /*< private >*/
    UfoBaseSchedulerClass parent_class;
};

UfoBaseScheduler    *ufo_fixed_scheduler_new            (UfoConfig  *config);
UfoArchGraph        *ufo_fixed_scheduler_get_arch       (UfoFixedScheduler *sched);
GType                ufo_fixed_scheduler_get_type       (void);
GQuark               ufo_fixed_scheduler_error_quark    (void);

G_END_DECLS

#endif
