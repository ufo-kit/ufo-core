#include <ufo-gpu-task-iface.h>

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

void
ufo_gpu_task_reduce (UfoGpuTask *task,
                     UfoBuffer *output,
                     UfoRequisition *requisition,
                     UfoGpuNode *node)
{
    UFO_GPU_TASK_GET_IFACE (task)->reduce (task, output, requisition, node);
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
ufo_gpu_task_reduce_real (UfoGpuTask *task,
                          UfoBuffer *output,
                          UfoRequisition *requisition,
                          UfoGpuNode *node)
{
    g_warning ("`reduce' of UfoGpuTaskInterface not implemented");
}

static void
ufo_gpu_task_default_init (UfoGpuTaskInterface *iface)
{
    iface->process = ufo_gpu_task_process_real;
    iface->reduce = ufo_gpu_task_reduce_real;
}
