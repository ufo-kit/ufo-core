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
#include "ufo-duplicate-task.h"


struct _UfoDuplicateTaskPrivate {
    unsigned alloc_size;
    unsigned idx;
    UfoBuffer *data;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoDuplicateTask, ufo_duplicate_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_DUPLICATE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_DUPLICATE_TASK, UfoDuplicateTaskPrivate))

UfoNode *
ufo_duplicate_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_DUPLICATE_TASK, NULL));
}

static void
ufo_duplicate_task_setup (UfoTask *task,
                          UfoResources *resources,
                          GError **error)
{
}

static void
ufo_duplicate_task_get_requisition (UfoTask *task,
                                    UfoBuffer **inputs,
                                    UfoRequisition *requisition,
                                    GError **error)
{
    ufo_buffer_get_requisition(inputs[0], requisition);
}

static guint
ufo_duplicate_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_duplicate_task_get_num_dimensions (UfoTask *task,
                                       guint input)
{
    return 2;
}

static UfoTaskMode
ufo_duplicate_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_REDUCTOR;
}

static gboolean
ufo_duplicate_task_process (UfoTask *task,
                            UfoBuffer **inputs,
                            UfoBuffer *output,
                            UfoRequisition *requisition)
{
    UfoDuplicateTaskPrivate *priv = UFO_DUPLICATE_TASK_GET_PRIVATE (task);

    if (priv->idx > priv->alloc_size) {
        priv->alloc_size *= 2;
        size_t nb_bytes = priv->alloc_size * sizeof (UfoBuffer *);
        priv->data = realloc (priv->data, nb_bytes);
    }
    return TRUE;
}

static void
ufo_duplicate_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_duplicate_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_duplicate_task_setup;
    iface->get_num_inputs = ufo_duplicate_task_get_num_inputs;
    iface->get_num_dimensions = ufo_duplicate_task_get_num_dimensions;
    iface->get_mode = ufo_duplicate_task_get_mode;
    iface->get_requisition = ufo_duplicate_task_get_requisition;
    iface->process = ufo_duplicate_task_process;
}

static void
ufo_duplicate_task_class_init (UfoDuplicateTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = ufo_duplicate_task_finalize;

    g_type_class_add_private (oclass, sizeof(UfoDuplicateTaskPrivate));
}

static void
ufo_duplicate_task_init(UfoDuplicateTask *self)
{
    self->priv = UFO_DUPLICATE_TASK_GET_PRIVATE(self);
    self->priv->alloc_size = 256;
    self->priv->data = malloc (sizeof (UfoBuffer *) * self->priv->alloc_size);
    self->priv->idx = 0;
}
