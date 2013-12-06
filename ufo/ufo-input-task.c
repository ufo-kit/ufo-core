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
#include "config.h"

#ifdef HAVE_PYTHON
#include <Python.h>
#endif

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include <gmodule.h>
#include <ufo/ufo-input-task.h>
#include <ufo/ufo-cpu-task-iface.h>

/**
 * SECTION:ufo-input-task
 * @Short_description: An input task
 * @Title: UfoInputTask
 */

struct _UfoInputTaskPrivate {
    GAsyncQueue *in_queue;
    GAsyncQueue *out_queue;
    UfoTaskMode mode;
    gboolean active;
    guint n_inputs;
    UfoInputParam *in_params;
    UfoBuffer *input;
};

static void ufo_task_interface_init (UfoTaskIface *iface);
static void ufo_cpu_task_interface_init (UfoCpuTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoInputTask, ufo_input_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init)
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_CPU_TASK,
                                                ufo_cpu_task_interface_init))

#define UFO_INPUT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_INPUT_TASK, UfoInputTaskPrivate))

enum {
    PROP_0,
    N_PROPERTIES
};

UfoNode *
ufo_input_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_INPUT_TASK, NULL));
}

void
ufo_input_task_stop (UfoInputTask *task)
{
    g_return_if_fail (UFO_IS_INPUT_TASK (task));
    task->priv->active = FALSE;
}

void
ufo_input_task_release_input_buffer (UfoInputTask *task,
                                     UfoBuffer *buffer)
{
    g_return_if_fail (UFO_IS_INPUT_TASK (task));
    g_async_queue_push (task->priv->in_queue, buffer);
}

/**
 * ufo_input_task_get_input_buffer:
 * @task: A #UfoInputTask
 *
 * Get the input buffer to which we write the data received from the master
 * remote node.
 *
 * Return value: (transfer none): A #UfoBuffer for writing input data.
 */
UfoBuffer *
ufo_input_task_get_input_buffer (UfoInputTask *task)
{
    UfoBuffer *buffer;
    g_return_val_if_fail (UFO_IS_INPUT_TASK (task), NULL);

#ifdef HAVE_PYTHON
    /*
     * We have to let the Python interpreter run its threads, because this
     * function here might block before Python code can insert any buffer.
     */
    Py_BEGIN_ALLOW_THREADS
#endif

    buffer = g_async_queue_pop (task->priv->out_queue);

#ifdef HAVE_PYTHON
    Py_END_ALLOW_THREADS
#endif

    return buffer;
}

static void
ufo_input_task_setup (UfoTask *task,
                      UfoResources *resources,
                      GError **error)
{
}

static void
ufo_input_task_get_structure (UfoTask *task,
                              guint *n_inputs,
                              UfoInputParam **in_params,
                              UfoTaskMode *mode)
{
    *n_inputs = 0;
    *mode = UFO_TASK_MODE_GENERATOR;
}

static void
ufo_input_task_get_requisition (UfoTask *task,
                                UfoBuffer **none,
                                UfoRequisition *requisition)
{
    UfoInputTaskPrivate *priv;

    priv = UFO_INPUT_TASK_GET_PRIVATE (task);

    /* Pop input here but release later in ufo_input_task_generate */
    if (priv->active) {
        priv->input = NULL;

        while (priv->active && priv->input == NULL)
            priv->input = g_async_queue_timeout_pop (priv->in_queue, G_USEC_PER_SEC / 10);

        if (priv->input == NULL)
            return;

        ufo_buffer_get_requisition (priv->input, requisition);
    }
}

static gboolean
ufo_input_task_generate (UfoCpuTask *task,
                         UfoBuffer *output,
                         UfoRequisition *requisition)
{
    UfoInputTaskPrivate *priv;

    g_return_val_if_fail (UFO_IS_INPUT_TASK (task), FALSE);
    priv = UFO_INPUT_TASK_GET_PRIVATE (task);

    if (!priv->active && priv->input == NULL)
        return FALSE;

    ufo_buffer_discard_location (output);
    ufo_buffer_copy (priv->input, output);

    /* input was popped in ufo_input_task_get_requisition */
    g_async_queue_push (priv->out_queue, priv->input);
    priv->input = NULL;

    return TRUE;
}

static void
ufo_input_task_finalize (GObject *object)
{
    UfoInputTaskPrivate *priv;

    priv = UFO_INPUT_TASK_GET_PRIVATE (object);
    g_async_queue_unref (priv->in_queue);
    g_async_queue_unref (priv->out_queue);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_input_task_setup;
    iface->get_structure = ufo_input_task_get_structure;
    iface->get_requisition = ufo_input_task_get_requisition;
}

static void
ufo_cpu_task_interface_init (UfoCpuTaskIface *iface)
{
    iface->generate = ufo_input_task_generate;
}

static void
ufo_input_task_class_init (UfoInputTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = ufo_input_task_finalize;

    g_type_class_add_private (gobject_class, sizeof(UfoInputTaskPrivate));
}

static void
ufo_input_task_init (UfoInputTask *task)
{
    task->priv = UFO_INPUT_TASK_GET_PRIVATE (task);
    ufo_task_node_set_plugin_name (UFO_TASK_NODE (task), "input-task");

    task->priv->in_queue = g_async_queue_new ();
    task->priv->out_queue = g_async_queue_new ();
    task->priv->active = TRUE;
}
