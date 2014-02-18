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

#include <gmodule.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <ufo/ufo-output-task.h>
#include <ufo/ufo-cpu-task-iface.h>

/**
 * SECTION:ufo-output-task
 * @Short_description: Output task
 * @Title: UfoOutputTask
 */

struct _UfoOutputTaskPrivate {
    GAsyncQueue *out_queue;
    GAsyncQueue *in_queue;
    guint n_dims;
    guint n_copies;
    GList *copies;
    GAsyncQueue *sorted_result_queue;
};

static volatile guint *last_buffer_position;

static void ufo_task_interface_init (UfoTaskIface *iface);
static void ufo_cpu_task_interface_init (UfoCpuTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoOutputTask, ufo_output_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init) G_IMPLEMENT_INTERFACE (UFO_TYPE_CPU_TASK, ufo_cpu_task_interface_init))

#define UFO_OUTPUT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_OUTPUT_TASK, UfoOutputTaskPrivate))

enum {
    PROP_0,
    N_PROPERTIES
};

UfoNode *
ufo_output_task_new (guint n_dims)
{
    UfoOutputTask *task = UFO_OUTPUT_TASK (g_object_new (UFO_TYPE_OUTPUT_TASK, NULL));

    task->priv->n_dims = n_dims;
    return UFO_NODE (task);
}

void
ufo_output_task_get_output_requisition (UfoOutputTask *task,
                                        UfoRequisition *requisition)
{
    UfoBuffer *buffer;
    UfoOutputTaskPrivate *priv;
    g_return_if_fail (UFO_IS_OUTPUT_TASK (task));

    priv = task->priv;
    buffer = g_async_queue_pop (priv->out_queue);
    ufo_buffer_get_requisition (buffer, requisition);
    g_async_queue_push (priv->out_queue, buffer);
}

static gint
compare_buffer_ids (gpointer a, gpointer b, gpointer *unused)
{
    gint id_a = ufo_buffer_get_id ((UfoBuffer *) a);
    gint id_b = ufo_buffer_get_id ((UfoBuffer *) b);

    if (id_a < id_b) return -1;
    if (id_a > id_b) return +1;
    g_assert (id_a != id_b);
    return 0;
}

/**
 * ufo_output_task_get_output_buffer:
 * @task: A #UfoInputTask
 *
 * Get the output buffer from which we read the data to be sent to the master
 * remote node.
 *
 * Return value: (transfer full): A #UfoBuffer for reading output data.
 */

UfoBuffer *
ufo_output_task_get_output_buffer (UfoOutputTask *task)
{
    UfoBuffer *buffer;
    g_return_val_if_fail (UFO_IS_OUTPUT_TASK (task), NULL);

    // this returns a result - not an empty buffer for reuse
    // we need to make sure the buffers leave the queue in their order they were received
    buffer = g_async_queue_pop (task->priv->sorted_result_queue);
    while (ufo_buffer_get_id (buffer) != (guint) g_atomic_int_get (last_buffer_position)) {
        g_async_queue_push_sorted (task->priv->sorted_result_queue,
                                   buffer,
                                   (GCompareDataFunc) compare_buffer_ids,
                                   NULL);
        buffer = g_async_queue_pop (task->priv->sorted_result_queue);
    }
    g_atomic_int_add (last_buffer_position, 1);

    return buffer;
}

void
ufo_output_task_release_output_buffer (UfoOutputTask *task,
                                       UfoBuffer *buffer)
{
    g_return_if_fail (UFO_IS_OUTPUT_TASK (task));
    g_async_queue_push (task->priv->in_queue, buffer);
}

static void
ufo_output_task_setup (UfoTask *task,
                       UfoResources *resources,
                       GError **error)
{
}

static void
ufo_output_task_get_requisition (UfoTask *task,
                                 UfoBuffer **inputs,
                                 UfoRequisition *requisition)
{
    (*requisition).n_dims = 0;
}

static void
ufo_output_task_get_structure (UfoTask *task,
                               guint *n_inputs,
                               UfoInputParam **in_params,
                               UfoTaskMode *mode)
{
    UfoOutputTaskPrivate *priv = UFO_OUTPUT_TASK_GET_PRIVATE (task);

    *mode = UFO_TASK_MODE_PROCESSOR;
    *n_inputs = 1;
    *in_params = g_new0 (UfoInputParam, 1);
    (*in_params)[0].n_dims = priv->n_dims;
}

static guint buffer_count = 0;
static gboolean
ufo_output_task_process (UfoCpuTask *task,
                         UfoBuffer **outputs,
                         UfoBuffer *output,
                         UfoRequisition *requisition)
{
    UfoOutputTaskPrivate *priv;
    UfoRequisition req;
    UfoBuffer *copy;

    g_return_val_if_fail (UFO_IS_OUTPUT_TASK (task), FALSE);
    ufo_buffer_get_requisition (outputs[0], &req);

    priv = UFO_OUTPUT_TASK_GET_PRIVATE (task);

    // creates a copy so that we have 2 buffer that circle between in and out queue
    if (priv->n_copies == 0) {
        copy = ufo_buffer_dup (outputs[0]);
        priv->copies = g_list_append (priv->copies, copy);
        priv->n_copies++;
    }
    else {
        copy = g_async_queue_pop (priv->in_queue);
    }

    ufo_buffer_copy (outputs[0], copy);
    g_async_queue_push (priv->out_queue, copy);

    // TODO avoid costly buffer copy?
    UfoBuffer *result_output = ufo_buffer_dup (copy);
    ufo_buffer_copy (copy, result_output);
    ufo_buffer_set_id (result_output, buffer_count++);
    g_async_queue_push_sorted (priv->sorted_result_queue,
                               result_output,
                               (GCompareDataFunc) compare_buffer_ids,
                               NULL);
    return TRUE;
}

static void
ufo_output_task_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_output_task_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_output_task_dispose (GObject *object)
{
    UfoOutputTaskPrivate *priv;

    priv = UFO_OUTPUT_TASK_GET_PRIVATE (object);
    g_list_foreach (priv->copies, (GFunc) g_object_unref, NULL);

    G_OBJECT_CLASS (ufo_output_task_parent_class)->dispose (object);
}

static void
ufo_output_task_finalize (GObject *object)
{
    UfoOutputTaskPrivate *priv;

    priv = UFO_OUTPUT_TASK_GET_PRIVATE (object);
    g_list_free (priv->copies);
    priv->copies = NULL;

    G_OBJECT_CLASS (ufo_output_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_output_task_setup;
    iface->get_structure = ufo_output_task_get_structure;
    iface->get_requisition = ufo_output_task_get_requisition;
}

static void
ufo_cpu_task_interface_init (UfoCpuTaskIface *iface)
{
    iface->process = ufo_output_task_process;
}

static void
ufo_output_task_class_init (UfoOutputTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_output_task_set_property;
    gobject_class->get_property = ufo_output_task_get_property;
    gobject_class->dispose = ufo_output_task_dispose;
    gobject_class->finalize = ufo_output_task_finalize;

    g_type_class_add_private (gobject_class, sizeof(UfoOutputTaskPrivate));

    last_buffer_position = g_malloc0 (sizeof(guint)); 
}

static void
ufo_output_task_init (UfoOutputTask *task)
{
    task->priv = UFO_OUTPUT_TASK_GET_PRIVATE (task);
    task->priv->out_queue = g_async_queue_new ();
    task->priv->in_queue = g_async_queue_new ();
    task->priv->sorted_result_queue = g_async_queue_new ();
    task->priv->n_copies = 0;
    task->priv->copies = NULL;
    *last_buffer_position = 0;

    ufo_task_node_set_plugin_name (UFO_TASK_NODE (task), "output-task");
}
