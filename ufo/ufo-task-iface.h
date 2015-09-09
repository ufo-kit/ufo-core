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

#include <json-glib/json-glib.h>
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

typedef enum {
    UFO_TASK_ERROR_SETUP
} UfoTaskError;

/**
 * UfoTaskMode:
 * @UFO_TASK_MODE_INVALID: invalid
 * @UFO_TASK_MODE_PROCESSOR: one-by-one processing
 * @UFO_TASK_MODE_GENERATOR: do not receive any data but produce a stream.
 * @UFO_TASK_MODE_REDUCTOR: receive fininite stream and generate a reduced stream
 * @UFO_TASK_MODE_SINK: receives data but does not produce any,
 * @UFO_TASK_MODE_GPU: runs on GPU
 * @UFO_TASK_MODE_CPU: runs on CPU
 * @UFO_TASK_MODE_SHARE_DATA: sibling tasks share the same input data
 * @UFO_TASK_MODE_TYPE_MASK: mask to get type from UfoTaskMode
 * @UFO_TASK_MODE_PROCESSOR_MASK: mask to get processor from UfoTaskMode
 *
 * Task modes describe how a task operates considering the input data.
 */
typedef enum {
    UFO_TASK_MODE_INVALID       = 0,
    UFO_TASK_MODE_PROCESSOR     = 1 << 0,
    UFO_TASK_MODE_GENERATOR     = 1 << 1,
    UFO_TASK_MODE_REDUCTOR      = 1 << 2,
    UFO_TASK_MODE_SINK          = 1 << 3,
    UFO_TASK_MODE_CPU           = 1 << 4,
    UFO_TASK_MODE_GPU           = 1 << 5,
    UFO_TASK_MODE_SHARE_DATA    = 1 << 6,

    UFO_TASK_MODE_TYPE_MASK     = UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GENERATOR | UFO_TASK_MODE_REDUCTOR  | UFO_TASK_MODE_SINK,

    UFO_TASK_MODE_PROCESSOR_MASK = UFO_TASK_MODE_CPU | UFO_TASK_MODE_GPU
} UfoTaskMode;

typedef gboolean (*UfoTaskProcessFunc) (UfoTask *task,
                                        UfoBuffer **inputs,
                                        UfoBuffer *output,
                                        UfoRequisition *requisition);

typedef gboolean (*UfoTaskGenerateFunc) (UfoTask *task,
                                         UfoBuffer *output,
                                         UfoRequisition *requisition);

struct _UfoTaskIface {
    /*< private >*/
    GTypeInterface parent_iface;

    void    (*setup)                    (UfoTask        *task,
                                         UfoResources   *resources,
                                         GError        **error);
    guint   (*get_num_inputs)           (UfoTask        *task);
    guint   (*get_num_dimensions)       (UfoTask        *task,
                                         guint          input);
    UfoTaskMode
            (*get_mode)                 (UfoTask        *task);
    UfoTaskMode
            (*get_preferred_mode)       (UfoTask        *task);
    void    (*get_requisition)          (UfoTask        *task,
                                         UfoBuffer     **inputs,
                                         UfoRequisition *requisition);
    void    (*set_json_object_property) (UfoTask        *task,
                                         const gchar    *prop_name,
                                         JsonObject     *object);
    gboolean (*process)                 (UfoTask        *task,
                                         UfoBuffer     **inputs,
                                         UfoBuffer      *output,
                                         UfoRequisition *requisition);
    gboolean (*generate)                (UfoTask        *task,
                                         UfoBuffer      *output,
                                         UfoRequisition *requisition);
};

void    ufo_task_setup              (UfoTask        *task,
                                     UfoResources   *resources,
                                     GError        **error);
guint   ufo_task_get_num_inputs     (UfoTask        *task);
guint   ufo_task_get_num_dimensions (UfoTask        *task,
                                     guint           input);
UfoTaskMode
        ufo_task_get_mode           (UfoTask        *task);
UfoTaskMode
        ufo_task_get_preferred_mode (UfoTask        *task);
void    ufo_task_get_requisition    (UfoTask        *task,
                                     UfoBuffer     **inputs,
                                     UfoRequisition *requisition);
void    ufo_task_set_json_object_property
                                    (UfoTask        *task,
                                     const gchar    *prop_name,
                                     JsonObject     *object);
gboolean ufo_task_process           (UfoTask        *task,
                                     UfoBuffer     **inputs,
                                     UfoBuffer      *output,
                                     UfoRequisition *requisition);
gboolean ufo_task_generate          (UfoTask        *task,
                                     UfoBuffer      *output,
                                     UfoRequisition *requisition);
gboolean ufo_task_uses_gpu          (UfoTask        *task);
gboolean ufo_task_uses_cpu          (UfoTask        *task);

GQuark ufo_task_error_quark     (void);
GType  ufo_task_get_type        (void);

G_END_DECLS

#endif
