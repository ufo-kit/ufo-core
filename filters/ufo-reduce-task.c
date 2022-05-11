/*
 * Copyright (C) 2011-2015 Karlsruhe Institute of Technology
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
 *
 * Authored by: Alexandre Lewkowicz (lewkow_a@epita.fr)
 */
#include "config.h"

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "ufo-reduce-task.h"


static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoReduceTask, ufo_reduce_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_REDUCE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_REDUCE_TASK, UfoReduceTaskPrivate))


UfoNode *
ufo_reduce_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_REDUCE_TASK, NULL));
}

static void
ufo_reduce_task_setup (UfoTask *task,
                       UfoResources *resources,
                       GError **error)
{
}

static void
ufo_reduce_task_get_requisition (UfoTask *task,
                                 UfoBuffer **inputs,
                                 UfoRequisition *requisition,
                                 GError **error)
{
    ufo_buffer_get_requisition(inputs[0], requisition);
    requisition->dims[0] /= 2;
    requisition->dims[1] /= 2;
}

static guint
ufo_reduce_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_reduce_task_get_num_dimensions (UfoTask *task,
                                             guint input)
{
    return 2;
}

static UfoTaskMode
ufo_reduce_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_CPU;
}

static gboolean
ufo_reduce_task_process (UfoTask *task,
                         UfoBuffer **inputs,
                         UfoBuffer *output,
                         UfoRequisition *requisition)
{
    UfoRequisition input_req;
    ufo_buffer_get_requisition(inputs[0], &input_req);
    float *src = ufo_buffer_get_host_array(inputs[0], NULL);
    float *out = ufo_buffer_get_host_array(output, NULL);

    for (unsigned i = 0; i < requisition->dims[0]; ++i) {
        for (unsigned j = 0; j < requisition->dims[1]; ++j) {
            out[i + j * requisition->dims[0]] = src[2 * i + 2 * j * input_req.dims[0]];
            out[i + j * requisition->dims[0]] += src[2 * i + 1 + 2 * j * input_req.dims[0]];
            out[i + j * requisition->dims[0]] += src[2 * i + j * 2 * input_req.dims[0] + input_req.dims[0]];
            out[i + j * requisition->dims[0]] += src[2 * i + 1 + j * 2 * input_req.dims[0] + input_req.dims[0]];
        }
    }

    return TRUE;
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_reduce_task_setup;
    iface->get_num_inputs = ufo_reduce_task_get_num_inputs;
    iface->get_num_dimensions = ufo_reduce_task_get_num_dimensions;
    iface->get_mode = ufo_reduce_task_get_mode;
    iface->get_requisition = ufo_reduce_task_get_requisition;
    iface->process = ufo_reduce_task_process;
}

static void
ufo_reduce_task_class_init (UfoReduceTaskClass *klass)
{
}

static void
ufo_reduce_task_init(UfoReduceTask *self)
{
}
