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

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <string.h>
#include "ufo-merge-task.h"


struct _UfoMergeTaskPrivate {
    guint num_inputs;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoMergeTask, ufo_merge_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_MERGE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_MERGE_TASK, UfoMergeTaskPrivate))

enum {
    PROP_0,
    PROP_NUM_INPUTS,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_merge_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_MERGE_TASK, NULL));
}

static void
ufo_merge_task_setup (UfoTask *task,
                      UfoResources *resources,
                      GError **error)
{
}

static void
ufo_merge_task_get_requisition (UfoTask *task,
                                UfoBuffer **inputs,
                                UfoRequisition *requisition,
                                GError **error)
{
    UfoMergeTaskPrivate *priv;

    priv = UFO_MERGE_TASK_GET_PRIVATE (task);
    requisition->n_dims = 2;
    requisition->dims[0] = 0;
    requisition->dims[1] = 0;

    for (guint i = 0; i < priv->num_inputs; i++) {
        UfoRequisition req;

        ufo_buffer_get_requisition (inputs[i], &req);
        requisition->dims[0] = MAX (requisition->dims[0], req.dims[0]);
        requisition->dims[1] += req.dims[1];
    }
}

static guint
ufo_merge_task_get_num_inputs (UfoTask *task)
{
    UfoMergeTaskPrivate *priv;

    priv = UFO_MERGE_TASK_GET_PRIVATE (task);
    return priv->num_inputs;
}

static guint
ufo_merge_task_get_num_dimensions (UfoTask *task,
                                   guint input)
{
    return 2;
}

static UfoTaskMode
ufo_merge_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_CPU;
}

static gboolean
ufo_merge_task_process (UfoTask *task,
                        UfoBuffer **inputs,
                        UfoBuffer *output,
                        UfoRequisition *requisition)
{
    UfoMergeTaskPrivate *priv;
    gchar *dest_host_mem;
    gsize offset;

    priv = UFO_MERGE_TASK_GET_PRIVATE (task);

    offset = 0;
    dest_host_mem = (gchar *) ufo_buffer_get_host_array (output, NULL);

    for (guint i = 0; i < priv->num_inputs; i++) {
        gsize size;
        gchar *src_host_mem;

        size = ufo_buffer_get_size (inputs[i]);
        src_host_mem = (gchar *) ufo_buffer_get_host_array (inputs[i], NULL);
        memmove (dest_host_mem + offset, src_host_mem, size);
        offset += size;
    }

    return TRUE;
}

static void
ufo_merge_task_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    UfoMergeTaskPrivate *priv = UFO_MERGE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_INPUTS:
            priv->num_inputs = g_value_get_uint (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_merge_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoMergeTaskPrivate *priv = UFO_MERGE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_INPUTS:
            g_value_set_uint (value, priv->num_inputs);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_merge_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_merge_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_merge_task_setup;
    iface->get_num_inputs = ufo_merge_task_get_num_inputs;
    iface->get_num_dimensions = ufo_merge_task_get_num_dimensions;
    iface->get_mode = ufo_merge_task_get_mode;
    iface->get_requisition = ufo_merge_task_get_requisition;
    iface->process = ufo_merge_task_process;
}

static void
ufo_merge_task_class_init (UfoMergeTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_merge_task_set_property;
    oclass->get_property = ufo_merge_task_get_property;
    oclass->finalize = ufo_merge_task_finalize;

    properties[PROP_NUM_INPUTS] =
        g_param_spec_uint ("number",
            "Number of inputs",
            "Number of inputs",
            2, 16, 2,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoMergeTaskPrivate));
}

static void
ufo_merge_task_init(UfoMergeTask *self)
{
    self->priv = UFO_MERGE_TASK_GET_PRIVATE(self);
    self->priv->num_inputs = 2;
}
