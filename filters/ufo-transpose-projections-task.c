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

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include <string.h>

#include "ufo-transpose-projections-task.h"


struct _UfoTransposeProjectionsTaskPrivate {
    guint n_projections;
    gfloat *sinograms;
    gsize projection;
    gsize sino_offset;
    guint current_sino;
    guint n_sinos;
    guint sino_width;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoTransposeProjectionsTask, ufo_transpose_projections_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_TRANSPOSE_PROJECTIONS_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_TRANSPOSE_PROJECTIONS_TASK, UfoTransposeProjectionsTaskPrivate))

enum {
    PROP_0,
    PROP_NUM_PROJECTIONS,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_transpose_projections_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_TRANSPOSE_PROJECTIONS_TASK, NULL));
}

static gboolean
ufo_transpose_projections_task_process (UfoTask *task,
                                 UfoBuffer **inputs,
                                 UfoBuffer *output,
                                 UfoRequisition *requisition)
{
    UfoTransposeProjectionsTaskPrivate *priv;
    gsize sino_index;
    gsize row_mem_offset;
    gsize sino_mem_offset;
    gfloat *host_array;
    guint i;

    priv = UFO_TRANSPOSE_PROJECTIONS_TASK_GET_PRIVATE (task);

    if (priv->projection > priv->n_projections)
        return FALSE;

    sino_index = (priv->projection - 1) * priv->sino_width;
    host_array = ufo_buffer_get_host_array (inputs[0], NULL);
    row_mem_offset = priv->sino_width;
    sino_mem_offset = row_mem_offset * priv->n_projections;

#pragma omp parallel
    {
#pragma omp for
        for (i = 0; i < priv->n_sinos; i++) {
            memcpy (priv->sinograms + sino_index + i * sino_mem_offset,
                    host_array + i * row_mem_offset,
                    sizeof (float) * priv->sino_width);
        }
    }

    priv->projection++;
    return TRUE;
}

static gboolean
ufo_transpose_projections_task_generate (UfoTask *task,
                                  UfoBuffer *output,
                                  UfoRequisition *requisition)
{
    UfoTransposeProjectionsTaskPrivate *priv;
    gsize index;

    priv = UFO_TRANSPOSE_PROJECTIONS_TASK_GET_PRIVATE (task);

    if (priv->current_sino == priv->n_sinos)
        return FALSE;

    index = priv->current_sino * priv->sino_offset;
    ufo_buffer_set_host_array (output, priv->sinograms + index, FALSE);

    priv->current_sino++;
    return TRUE;
}

static void
ufo_transpose_projections_task_setup (UfoTask *task,
                               UfoResources *resources,
                               GError **error)
{
}

static void
ufo_transpose_projections_task_get_requisition (UfoTask *task,
                                                UfoBuffer **inputs,
                                                UfoRequisition *requisition,
                                                GError **error)
{
    UfoTransposeProjectionsTaskPrivate *priv;
    UfoRequisition in_req;

    priv = UFO_TRANSPOSE_PROJECTIONS_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], &in_req);
    requisition->n_dims = 2;
    requisition->dims[0] = in_req.dims[0];
    requisition->dims[1] = priv->n_projections;

    if (priv->sinograms == NULL) {
        priv->sino_width = (guint) in_req.dims[0];
        priv->n_sinos = (guint) in_req.dims[1];
        priv->sinograms = g_malloc0 (sizeof (gfloat) * priv->n_projections * priv->sino_width * priv->n_sinos);
        priv->sino_offset = priv->sino_width * priv->n_projections;
        priv->current_sino = 0;
        priv->projection = 1;
    }
}

static guint
ufo_transpose_projections_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_transpose_projections_task_get_num_dimensions (UfoTask *task,
                               guint input)
{
    g_return_val_if_fail (input == 0, 0);
    return 2;
}

static UfoTaskMode
ufo_transpose_projections_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_REDUCTOR | UFO_TASK_MODE_CPU;
}


static gboolean
ufo_transpose_projections_task_equal_real (UfoNode *n1,
                                    UfoNode *n2)
{
    g_return_val_if_fail (UFO_IS_TRANSPOSE_PROJECTIONS_TASK (n1) && UFO_IS_TRANSPOSE_PROJECTIONS_TASK (n2), FALSE);
    return TRUE;
}

static void
ufo_transpose_projections_task_finalize (GObject *object)
{
    UfoTransposeProjectionsTaskPrivate *priv;

    priv = UFO_TRANSPOSE_PROJECTIONS_TASK_GET_PRIVATE (object);

    if (priv->sinograms) {
        g_free (priv->sinograms);
        priv->sinograms = NULL;
    }
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_transpose_projections_task_setup;
    iface->get_requisition = ufo_transpose_projections_task_get_requisition;
    iface->get_num_inputs = ufo_transpose_projections_task_get_num_inputs;
    iface->get_num_dimensions = ufo_transpose_projections_task_get_num_dimensions;
    iface->get_mode = ufo_transpose_projections_task_get_mode;
    iface->process = ufo_transpose_projections_task_process;
    iface->generate = ufo_transpose_projections_task_generate;
}

static void
ufo_transpose_projections_task_set_property (GObject *object,
                                      guint property_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
    UfoTransposeProjectionsTaskPrivate *priv = UFO_TRANSPOSE_PROJECTIONS_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_PROJECTIONS:
            priv->n_projections = g_value_get_uint (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_transpose_projections_task_get_property (GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
    UfoTransposeProjectionsTaskPrivate *priv = UFO_TRANSPOSE_PROJECTIONS_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_PROJECTIONS:
            g_value_set_uint (value, priv->n_projections);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_transpose_projections_task_class_init (UfoTransposeProjectionsTaskClass *klass)
{
    GObjectClass *oclass;
    UfoNodeClass *node_class;
    
    oclass = G_OBJECT_CLASS (klass);
    node_class = UFO_NODE_CLASS (klass);

    oclass->finalize = ufo_transpose_projections_task_finalize;
    oclass->set_property = ufo_transpose_projections_task_set_property;
    oclass->get_property = ufo_transpose_projections_task_get_property;

    properties[PROP_NUM_PROJECTIONS] = 
        g_param_spec_uint ("number",
            "Number of projections",
            "Number of projections",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    node_class->equal = ufo_transpose_projections_task_equal_real;

    g_type_class_add_private(klass, sizeof(UfoTransposeProjectionsTaskPrivate));
}

static void
ufo_transpose_projections_task_init (UfoTransposeProjectionsTask *self)
{
    UfoTransposeProjectionsTaskPrivate *priv;
    self->priv = priv = UFO_TRANSPOSE_PROJECTIONS_TASK_GET_PRIVATE (self);
    priv->sinograms = NULL;
    priv->n_projections = 1;
}
