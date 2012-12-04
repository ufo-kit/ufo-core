#include "ufo-gpu-task-iface.h"

typedef UfoGpuTaskIface UfoGpuTaskInterface;

G_DEFINE_INTERFACE (UfoGpuTask, ufo_gpu_task, UFO_TYPE_TASK)

gboolean
ufo_gpu_task_process (UfoGpuTask *task,
                      UfoBuffer **inputs,
                      UfoBuffer *output,
                      UfoRequisition *requisition,
                      UfoGpuNode *node)
{
    return UFO_GPU_TASK_GET_IFACE (task)->process (task, inputs, output, requisition, node);
}

static gboolean
ufo_gpu_task_process_real (UfoGpuTask *task,
                           UfoBuffer **inputs,
                           UfoBuffer *output,
                           UfoRequisition *requisition,
                           UfoGpuNode *node)
{
    g_warning ("`process' of UfoGpuTaskInterface not implemented");
    return FALSE;
}

static void
ufo_gpu_task_default_init (UfoGpuTaskInterface *iface)
{
    iface->process = ufo_gpu_task_process_real;
}
