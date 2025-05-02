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
 */
#include "config.h"

#include "ufo-refeed-task.h"


struct _UfoRefeedTaskPrivate {
    GList *buffers;
    GList *current;
    gboolean refeed;
    gboolean inserted;
    UfoRequisition requisition;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoRefeedTask, ufo_refeed_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_REFEED_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_REFEED_TASK, UfoRefeedTaskPrivate))

UfoNode *
ufo_refeed_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_REFEED_TASK, NULL));
}

static void
ufo_refeed_task_setup (UfoTask *task,
                       UfoResources *resources,
                       GError **error)
{
    UfoRefeedTaskPrivate *priv;

    priv = UFO_REFEED_TASK_GET_PRIVATE (task);
    priv->current = priv->buffers;

    /* That ensures that refeed is set to FALSE only once */
    priv->refeed = priv->inserted;
    priv->inserted = TRUE;
}

static void
ufo_refeed_task_get_requisition (UfoTask *task,
                                 UfoBuffer **inputs,
                                 UfoRequisition *requisition,
                                 GError **error)
{
    UfoRefeedTaskPrivate *priv;

    priv = UFO_REFEED_TASK_GET_PRIVATE (task);

    if (!priv->refeed) {
        ufo_buffer_get_requisition (inputs[0], requisition);
        ufo_buffer_get_requisition (inputs[0], &priv->requisition);
    }
    else {
        requisition->n_dims = priv->requisition.n_dims;
        requisition->dims[0] = priv->requisition.dims[0];
        requisition->dims[1] = priv->requisition.dims[1];
        requisition->dims[2] = priv->requisition.dims[2];
    }
}

static guint
ufo_refeed_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_refeed_task_get_num_dimensions (UfoTask *task,
                                    guint input)
{
    return -1;
}

static UfoTaskMode
ufo_refeed_task_get_mode (UfoTask *task)
{
    UfoRefeedTaskPrivate *priv;

    priv = UFO_REFEED_TASK_GET_PRIVATE (task);
    return priv->refeed ? UFO_TASK_MODE_GENERATOR : UFO_TASK_MODE_PROCESSOR;
}

static gboolean
ufo_refeed_task_process (UfoTask *task,
                         UfoBuffer **inputs,
                         UfoBuffer *output,
                         UfoRequisition *requisition)
{
    UfoRefeedTaskPrivate *priv;

    priv = UFO_REFEED_TASK_GET_PRIVATE (task);

    priv->buffers = g_list_append (priv->buffers, ufo_buffer_dup (inputs[0]));
    ufo_buffer_copy (inputs[0], output);
    return TRUE;
}

static gboolean
ufo_refeed_task_generate (UfoTask *task,
                          UfoBuffer *output,
                          UfoRequisition *requisition)
{
    UfoRefeedTaskPrivate *priv;

    priv = UFO_REFEED_TASK_GET_PRIVATE (task);

    if (priv->current == NULL)
        return FALSE;

    ufo_buffer_copy (UFO_BUFFER (priv->current->data), output);
    priv->current = g_list_next (priv->current);
    return TRUE;
}

static void
ufo_refeed_task_dispose (GObject *object)
{
    UfoRefeedTaskPrivate *priv;

    priv = UFO_REFEED_TASK_GET_PRIVATE (object);
    g_list_foreach (priv->buffers, (GFunc) g_object_unref, NULL);

    G_OBJECT_CLASS (ufo_refeed_task_parent_class)->dispose (object);
}

static void
ufo_refeed_task_finalize (GObject *object)
{
    UfoRefeedTaskPrivate *priv;

    priv = UFO_REFEED_TASK_GET_PRIVATE (object);
    g_list_free (priv->buffers);

    G_OBJECT_CLASS (ufo_refeed_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_refeed_task_setup;
    iface->get_num_inputs = ufo_refeed_task_get_num_inputs;
    iface->get_num_dimensions = ufo_refeed_task_get_num_dimensions;
    iface->get_mode = ufo_refeed_task_get_mode;
    iface->get_requisition = ufo_refeed_task_get_requisition;
    iface->process = ufo_refeed_task_process;
    iface->generate = ufo_refeed_task_generate;
}

static void
ufo_refeed_task_class_init (UfoRefeedTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = ufo_refeed_task_finalize;
    oclass->dispose = ufo_refeed_task_dispose;

    g_type_class_add_private (oclass, sizeof(UfoRefeedTaskPrivate));
}

static void
ufo_refeed_task_init(UfoRefeedTask *self)
{
    self->priv = UFO_REFEED_TASK_GET_PRIVATE(self);
    self->priv->buffers = NULL;
    self->priv->refeed = FALSE;
    self->priv->inserted = FALSE;
}
