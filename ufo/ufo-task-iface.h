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

#ifndef UFO_TASK_IFACE_H
#define UFO_TASK_IFACE_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <ufo/ufo-buffer.h>
#include <ufo/ufo-resources.h>

G_BEGIN_DECLS

#define UFO_TYPE_TASK             (ufo_task_get_type())
#define UFO_TASK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_TASK, UfoTask))
#define UFO_TASK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_TASK, UfoTaskIface))
#define UFO_IS_TASK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_TASK))
#define UFO_IS_TASK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_TASK))
#define UFO_TASK_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE((inst), UFO_TYPE_TASK, UfoTaskIface))

#define UFO_TASK_ERROR            ufo_task_error_quark()

typedef struct _UfoTask         UfoTask;
typedef struct _UfoTaskIface    UfoTaskIface;
typedef struct _UfoInputParam   UfoInputParam;

typedef enum {
    UFO_TASK_ERROR_SETUP
} UfoTaskError;

/**
 * UfoTaskMode:
 * @UFO_TASK_MODE_PROCESSOR: one-by-one processing
 * @UFO_TASK_MODE_GENERATOR: do not receive any data but produce a stream.
 * @UFO_TASK_MODE_REDUCTOR: receive fininite stream and generate a reduced stream
 *
 * Task modes describe how a task operates considering the input data.
 */
typedef enum {
    UFO_TASK_MODE_PROCESSOR,
    UFO_TASK_MODE_GENERATOR,
    UFO_TASK_MODE_REDUCTOR,
} UfoTaskMode;

typedef gboolean (*UfoTaskProcessFunc) (UfoTask *task,
                                        UfoBuffer **inputs,
                                        UfoBuffer *output,
                                        UfoRequisition *requisition);

typedef gboolean (*UfoTaskGenerateFunc) (UfoTask *task,
                                         UfoBuffer *output,
                                         UfoRequisition *requisition);

/**
 * UfoInputParam:
 * @n_dims: Number of dimensions
 */
struct _UfoInputParam {
    guint n_dims;
};

struct _UfoTaskIface {
    /*< private >*/
    GTypeInterface parent_iface;

    void (*setup)           (UfoTask        *task,
                             UfoResources   *resources,
                             GError        **error);
    void (*get_structure)   (UfoTask        *task,
                             guint          *n_inputs,
                             UfoInputParam **in_params,
                             UfoTaskMode    *mode);
    void (*get_requisition) (UfoTask        *task,
                             UfoBuffer     **inputs,
                             UfoRequisition *requisition);
};

void   ufo_task_setup           (UfoTask          *task,
                                 UfoResources     *resources,
                                 GError          **error);
void   ufo_task_get_requisition (UfoTask          *task,
                                 UfoBuffer       **inputs,
                                 UfoRequisition   *requisition);
void   ufo_task_get_structure   (UfoTask          *task,
                                 guint            *n_inputs,
                                 UfoInputParam  **in_params,
                                 UfoTaskMode      *mode);

GQuark ufo_task_error_quark     (void);
GType  ufo_task_get_type        (void);

G_END_DECLS

#endif
