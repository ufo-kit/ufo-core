#ifndef UFO_CPU_TASK_IFACE_H
#define UFO_CPU_TASK_IFACE_H

#include <glib-object.h>
#include "ufo-task-iface.h"
#include "ufo-buffer.h"

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
    UfoTaskIface parent_iface;

    gboolean (*process) (UfoCpuTask *task, UfoBuffer **inputs, UfoBuffer *output, UfoRequisition *requisition);
};

gboolean    ufo_cpu_task_process (UfoCpuTask     *task,
                                  UfoBuffer     **inputs,
                                  UfoBuffer      *output,
                                  UfoRequisition *requisition);

GType ufo_cpu_task_get_type (void);

G_END_DECLS

#endif
