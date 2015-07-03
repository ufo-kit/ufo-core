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

#include <ufo/ufo-remote-task.h>
#include <ufo/ufo-remote-node.h>

/**
 * SECTION:ufo-remote-task
 * @Short_description: Encapsulate remote tasks
 * @Title: UfoRemoteTask
 */

struct _UfoRemoteTaskPrivate {
    UfoRemoteNode *remote;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoRemoteTask, ufo_remote_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_REMOTE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_REMOTE_TASK, UfoRemoteTaskPrivate))

enum {
    PROP_0,
    N_PROPERTIES
};

UfoNode *
ufo_remote_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_REMOTE_TASK, NULL));
}

static void
ufo_remote_task_setup (UfoTask *task,
                       UfoResources *resources,
                       GError **error)
{
    UfoRemoteTaskPrivate *priv;

    priv = UFO_REMOTE_TASK_GET_PRIVATE (UFO_REMOTE_TASK (task));
    priv->remote = UFO_REMOTE_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    g_assert (priv->remote != NULL);
    ufo_remote_node_get_num_gpus (priv->remote);
    ufo_remote_node_request_setup (priv->remote);
}

static void
ufo_remote_task_get_requisition (UfoTask *task,
                                 UfoBuffer **inputs,
                                 UfoRequisition *requisition)
{
    UfoRemoteTaskPrivate *priv;

    priv = UFO_REMOTE_TASK_GET_PRIVATE (UFO_REMOTE_TASK (task));

    /*
     * We send our input to the remote node which will execute immediately.
     * After remote execution, we will know the requisition of the _last_ remote
     * task node and can get it back.
     */
    ufo_remote_node_send_inputs (priv->remote, inputs);
    ufo_remote_node_get_requisition (priv->remote, requisition);
}

static guint
ufo_remote_task_get_num_inputs (UfoTask *task)
{
    return ufo_remote_node_get_num_inputs (UFO_REMOTE_TASK_GET_PRIVATE (task)->remote);
}

static guint
ufo_remote_task_get_num_dimensions (UfoTask *task,
                                    guint input)
{
    return ufo_remote_node_get_num_dimensions (UFO_REMOTE_TASK_GET_PRIVATE (task)->remote, input);
}

static guint
ufo_remote_task_get_mode (UfoTask *task)
{
    return ufo_remote_node_get_mode (UFO_REMOTE_TASK_GET_PRIVATE (task)->remote);
}

static gboolean
ufo_remote_task_process (UfoTask *task,
                         UfoBuffer **inputs,
                         UfoBuffer *output,
                         UfoRequisition *requisition)
{
    UfoRemoteTaskPrivate *priv;
    priv = UFO_REMOTE_TASK_GET_PRIVATE (UFO_REMOTE_TASK (task));

    ufo_remote_node_get_result (priv->remote, output);
    g_debug ("remote: received result");
    return TRUE;
}

static void
ufo_remote_task_dispose (GObject *object)
{
    G_OBJECT_CLASS (ufo_remote_task_parent_class)->dispose (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_remote_task_setup;
    iface->get_num_inputs = ufo_remote_task_get_num_inputs;
    iface->get_num_dimensions = ufo_remote_task_get_num_dimensions;
    iface->get_mode = ufo_remote_task_get_mode;
    iface->get_requisition = ufo_remote_task_get_requisition;
    iface->process = ufo_remote_task_process;
}

static void
ufo_remote_task_class_init (UfoRemoteTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->dispose = ufo_remote_task_dispose;
    g_type_class_add_private (oclass, sizeof(UfoRemoteTaskPrivate));
}

static void
ufo_remote_task_init(UfoRemoteTask *self)
{
    self->priv = UFO_REMOTE_TASK_GET_PRIVATE(self);
}
