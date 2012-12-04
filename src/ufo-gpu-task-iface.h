#ifndef UFO_GPU_TASK_IFACE_H
#define UFO_GPU_TASK_IFACE_H

#include <glib-object.h>
#include "ufo-gpu-node.h"
#include "ufo-task-iface.h"
#include "ufo-buffer.h"

G_BEGIN_DECLS

#define UFO_TYPE_GPU_TASK             (ufo_gpu_task_get_type())
#define UFO_GPU_TASK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_GPU_TASK, UfoGpuTask))
#define UFO_GPU_TASK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_GPU_TASK, UfoGpuTaskIface))
#define UFO_IS_GPU_TASK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_GPU_TASK))
#define UFO_IS_GPU_TASK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_GPU_TASK))
#define UFO_GPU_TASK_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE((inst), UFO_TYPE_GPU_TASK, UfoGpuTaskIface))

typedef struct _UfoGpuTask         UfoGpuTask;
typedef struct _UfoGpuTaskIface    UfoGpuTaskIface;

struct _UfoGpuTaskIface {
    UfoTaskIface parent_iface;

    gboolean (*process) (UfoGpuTask *task, UfoBuffer **inputs, UfoBuffer *output, UfoRequisition *requisition, UfoGpuNode *node);
};

gboolean ufo_gpu_task_process (UfoGpuTask       *task,
                               UfoBuffer       **inputs,
                               UfoBuffer        *output,
                               UfoRequisition   *requisition,
                               UfoGpuNode       *node);

GType ufo_gpu_task_get_type (void);

G_END_DECLS

#endif
