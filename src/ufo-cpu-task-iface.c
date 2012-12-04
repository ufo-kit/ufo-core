#include "ufo-cpu-task-iface.h"

typedef UfoCpuTaskIface UfoCpuTaskInterface;

G_DEFINE_INTERFACE (UfoCpuTask, ufo_cpu_task, UFO_TYPE_TASK)


gboolean
ufo_cpu_task_process (UfoCpuTask *task,
                      UfoBuffer **inputs,
                      UfoBuffer *output,
                      UfoRequisition *requisition)
{
    return UFO_CPU_TASK_GET_IFACE (task)->process (task, inputs, output, requisition);
}

static gboolean
ufo_cpu_task_process_real (UfoCpuTask *task,
                           UfoBuffer **inputs,
                           UfoBuffer *output,
                           UfoRequisition *requisition)
{
    g_warning ("`process' of UfoCpuTaskInterface not implemented");
    return FALSE;
}

static void
ufo_cpu_task_default_init (UfoCpuTaskInterface *iface)
{
    iface->process = ufo_cpu_task_process_real;
}
