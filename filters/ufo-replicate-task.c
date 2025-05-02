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

#include <stdlib.h>
#include "ufo-replicate-task.h"

struct _UfoReplicateTaskPrivate {
    unsigned alloc_size;
    unsigned idx;
    UfoBuffer *data;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoReplicateTask, ufo_replicate_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_REPLICATE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_REPLICATE_TASK, UfoReplicateTaskPrivate))

UfoNode *
ufo_replicate_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_REPLICATE_TASK, NULL));
}

static void
ufo_replicate_task_setup (UfoTask *task,
                          UfoResources *resources,
                          GError **error)
{
}

static void
ufo_replicate_task_get_requisition (UfoTask *task,
                                    UfoBuffer **inputs,
                                    UfoRequisition *requisition,
                                    GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_replicate_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_replicate_task_get_num_dimensions (UfoTask *task,
                                       guint input)
{
    return 2;
}

static UfoTaskMode
ufo_replicate_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_REDUCTOR;
}

static gboolean
ufo_replicate_task_process (UfoTask *task,
                            UfoBuffer **inputs,
                            UfoBuffer *output,
                            UfoRequisition *requisition)
{
    UfoReplicateTaskPrivate *priv = UFO_REPLICATE_TASK_GET_PRIVATE (task);

    if (priv->idx > priv->alloc_size) {
        priv->alloc_size *= 2;
        size_t nb_bytes = priv->alloc_size * sizeof (UfoBuffer *);
        priv->data = realloc (priv->data, nb_bytes);
    }

    return TRUE;
}

static void
ufo_replicate_task_finalize (GObject *object)
{
    UfoReplicateTaskPrivate *priv;

    priv = UFO_REPLICATE_TASK_GET_PRIVATE (object);
    g_free (priv->data);
    G_OBJECT_CLASS (ufo_replicate_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_replicate_task_setup;
    iface->get_num_inputs = ufo_replicate_task_get_num_inputs;
    iface->get_num_dimensions = ufo_replicate_task_get_num_dimensions;
    iface->get_mode = ufo_replicate_task_get_mode;
    iface->get_requisition = ufo_replicate_task_get_requisition;
    iface->process = ufo_replicate_task_process;
}

static void
ufo_replicate_task_class_init (UfoReplicateTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = ufo_replicate_task_finalize;

    g_type_class_add_private (oclass, sizeof(UfoReplicateTaskPrivate));
}

static void
ufo_replicate_task_init(UfoReplicateTask *self)
{
    self->priv = UFO_REPLICATE_TASK_GET_PRIVATE(self);
    self->priv->alloc_size = 256;
    self->priv->data = g_malloc (sizeof (UfoBuffer *) * self->priv->alloc_size);
    self->priv->idx = 0;
}
