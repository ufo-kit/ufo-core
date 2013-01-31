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

#ifndef UFO_CPU_TASK_IFACE_H
#define UFO_CPU_TASK_IFACE_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <ufo-task-iface.h>
#include <ufo-buffer.h>

G_BEGIN_DECLS

#define UFO_TYPE_CPU_TASK             (ufo_cpu_task_get_type())
#define UFO_CPU_TASK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_CPU_TASK, UfoCpuTask))
#define UFO_CPU_TASK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_CPU_TASK, UfoCpuTaskIface))
#define UFO_IS_CPU_TASK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_CPU_TASK))
#define UFO_IS_CPU_TASK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_CPU_TASK))
#define UFO_CPU_TASK_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE((inst), UFO_TYPE_CPU_TASK, UfoCpuTaskIface))

typedef struct _UfoCpuTask         UfoCpuTask;
typedef struct _UfoCpuTaskIface    UfoCpuTaskIface;

struct _UfoCpuTaskIface {
    /*< private >*/
    UfoTaskIface parent_iface;

    gboolean (*process) (UfoCpuTask *task,
                         UfoBuffer **inputs,
                         UfoBuffer *output,
                         UfoRequisition *requisition);
    void     (*reduce)  (UfoCpuTask *task,
                         UfoBuffer *output,
                         UfoRequisition *requisition);
    gboolean (*generate)(UfoCpuTask *task,
                         UfoBuffer *output,
                         UfoRequisition *requisition);
};

gboolean    ufo_cpu_task_process (UfoCpuTask     *task,
                                  UfoBuffer     **inputs,
                                  UfoBuffer      *output,
                                  UfoRequisition *requisition);
void        ufo_cpu_task_reduce  (UfoCpuTask     *task,
                                  UfoBuffer      *output,
                                  UfoRequisition *requisition);
gboolean    ufo_cpu_task_generate(UfoCpuTask     *task,
                                  UfoBuffer      *output,
                                  UfoRequisition *requisition);

GType ufo_cpu_task_get_type (void);

G_END_DECLS

#endif
