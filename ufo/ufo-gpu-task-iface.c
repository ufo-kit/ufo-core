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

#include <ufo/ufo-gpu-task-iface.h>

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

gboolean
ufo_gpu_task_generate (UfoGpuTask *task,
                       UfoBuffer *output,
                       UfoRequisition *requisition,
                       UfoGpuNode *node)
{
    return UFO_GPU_TASK_GET_IFACE (task)->generate (task, output, requisition, node);
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

static gboolean
ufo_gpu_task_generate_real (UfoGpuTask *task,
                            UfoBuffer *output,
                            UfoRequisition *requisition,
                            UfoGpuNode *node)
{
    g_warning ("`generate' of UfoGpuTaskInterface not implemented");
    return FALSE;
}

static void
ufo_gpu_task_default_init (UfoGpuTaskInterface *iface)
{
    iface->process = ufo_gpu_task_process_real;
    iface->reduce = ufo_gpu_task_reduce_real;
    iface->generate = ufo_gpu_task_generate_real;
}
