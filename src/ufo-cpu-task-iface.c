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

#include <ufo-cpu-task-iface.h>

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

void
ufo_cpu_task_reduce (UfoCpuTask *task,
                     UfoBuffer *output,
                     UfoRequisition *requisition)
{
    UFO_CPU_TASK_GET_IFACE (task)->reduce (task, output, requisition);
}

gboolean
ufo_cpu_task_generate (UfoCpuTask *task,
                       UfoBuffer *output,
                       UfoRequisition *requisition)
{
    return UFO_CPU_TASK_GET_IFACE (task)->generate (task, output, requisition);
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
ufo_cpu_task_reduce_real (UfoCpuTask *task,
                          UfoBuffer *output,
                          UfoRequisition *requisition)
{
    g_warning ("`reduce' of UfoCpuTaskInterface not implemented");
}

static gboolean
ufo_cpu_task_generate_real (UfoCpuTask *task,
                            UfoBuffer *output,
                            UfoRequisition *requisition)
{
    g_warning ("`generate' of UfoCpuTaskInterface not implemented");
    return FALSE;
}

static void
ufo_cpu_task_default_init (UfoCpuTaskInterface *iface)
{
    iface->process = ufo_cpu_task_process_real;
    iface->reduce = ufo_cpu_task_reduce_real;
    iface->generate = ufo_cpu_task_generate_real;
}
