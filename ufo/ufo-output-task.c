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
#include <ufo/ufo-task-iface.h>

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
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoOutputTask, ufo_output_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_OUTPUT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_OUTPUT_TASK, UfoOutputTaskPrivate))

enum {
    PROP_0,
    PROP_NUM_DIMS,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

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
    g_return_val_if_fail (UFO_IS_OUTPUT_TASK (task), NULL);
    return g_object_ref (g_async_queue_pop (task->priv->out_queue));
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

static guint
ufo_output_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_output_task_get_num_dimensions (UfoTask *task,
                                    guint input)
{
    g_return_val_if_fail (input == 0, 0);
    return UFO_OUTPUT_TASK_GET_PRIVATE (task)->n_dims;
}

static UfoTaskMode
ufo_output_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_CPU;
}

static gboolean
ufo_output_task_process (UfoTask *task,
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
    return TRUE;
}

static void
ufo_output_task_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    UfoOutputTaskPrivate *priv;

    priv = UFO_OUTPUT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_DIMS:
            priv->n_dims = g_value_get_uint (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_output_task_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    switch (property_id) {
        case PROP_NUM_DIMS:
            g_value_set_uint (value, UFO_OUTPUT_TASK_GET_PRIVATE (object)->n_dims);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
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
    iface->get_num_inputs = ufo_output_task_get_num_inputs;
    iface->get_num_dimensions = ufo_output_task_get_num_dimensions;
    iface->get_mode = ufo_output_task_get_mode;
    iface->get_requisition = ufo_output_task_get_requisition;
    iface->process = ufo_output_task_process;
}

static void
ufo_output_task_class_init (UfoOutputTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_output_task_set_property;
    oclass->get_property = ufo_output_task_get_property;
    oclass->dispose = ufo_output_task_dispose;
    oclass->finalize = ufo_output_task_finalize;

    properties[PROP_NUM_DIMS] =
        g_param_spec_uint ("num-dims",
                           "Number of expected dimensions",
                           "Number of expected dimensions",
                           1, 3, 2, G_PARAM_READWRITE);

    g_object_class_install_property (oclass, PROP_NUM_DIMS, properties[PROP_NUM_DIMS]);

    g_type_class_add_private (oclass, sizeof(UfoOutputTaskPrivate));
}

static void
ufo_output_task_init (UfoOutputTask *task)
{
    task->priv = UFO_OUTPUT_TASK_GET_PRIVATE (task);
    task->priv->out_queue = g_async_queue_new ();
    task->priv->in_queue = g_async_queue_new ();
    task->priv->n_copies = 0;
    task->priv->copies = NULL;
    task->priv->n_dims = 2;

    ufo_task_node_set_plugin_name (UFO_TASK_NODE (task), "output-task");
}
