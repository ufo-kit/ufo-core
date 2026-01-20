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

#include <stdlib.h>
#include "ufo-flatten-task.h"

typedef enum {
    M_0,
    M_MEDIAN,
    M_LAST
} Mode;

static const gchar *modes[] = {"median"};

struct _UfoFlattenTaskPrivate {
    Mode mode;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoFlattenTask, ufo_flatten_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_FLATTEN_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FLATTEN_TASK, UfoFlattenTaskPrivate))

enum {
    PROP_0,
    PROP_MODE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_flatten_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_FLATTEN_TASK, NULL));
}

static Mode
string_to_mode (const gchar *s)
{
    for (Mode i = M_0 + 1; i < M_LAST; i++) {
        if (!g_strcmp0 (s, modes[i - 1]))
            return i;
    }

    return M_0;
}

static void
ufo_flatten_task_setup (UfoTask *task,
                        UfoResources *resources,
                        GError **error)
{
}

static void
ufo_flatten_task_get_requisition (UfoTask *task,
                                  UfoBuffer **inputs,
                                  UfoRequisition *requisition,
                                  GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
    requisition->n_dims = 2;
}

static guint
ufo_flatten_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_flatten_task_get_num_dimensions (UfoTask *task,
                                     guint input)
{
    return 3;
}

static UfoTaskMode
ufo_flatten_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_CPU;
}

static int
cmp_float (gconstpointer p1, gconstpointer p2)
{
    return (int) (*((const gfloat *) p1) - *((const gfloat *) p2));
}

static gboolean
ufo_flatten_task_process (UfoTask *task,
                          UfoBuffer **inputs,
                          UfoBuffer *output,
                          UfoRequisition *requisition)
{
    gfloat *in_mem;
    gfloat *out_mem;
    gfloat *tmp;
    gsize width, height, depth;
    UfoRequisition in_req;

    ufo_buffer_get_requisition (inputs[0], &in_req);
    if (in_req.n_dims != 3) {
        g_warning ("Flatten task requires a 3D input");
        return TRUE;
    }

    in_mem = ufo_buffer_get_host_array (inputs[0], NULL);
    out_mem = ufo_buffer_get_host_array (output, NULL);
    width = requisition->dims[0];
    height = requisition->dims[1];
    depth = requisition->dims[2];
    tmp = g_malloc (sizeof (gfloat) * depth);

    /*
     * For now, only median is necessary to flatten in a "fat" way, i.e. not
     * doing in-place. In fact we should replace the averager with a "thin"
     * flattener that can also determine the sum, min and max.
     */
    for (gsize x = 0; x < width; x++) {
        for (gsize y = 0; y < height; y++) {
            for (gsize i = 0; i < depth; i++)
                tmp[i] = in_mem[i * width * height + y * width + x];

            qsort ((gpointer) tmp, depth, sizeof(gfloat), cmp_float);
            if (depth % 2) {
                out_mem[y * width + x] = tmp[depth / 2];
            } else {
                /* Average two middle values if depth is even */
                out_mem[y * width + x] = (tmp[depth / 2] + tmp[depth / 2 - 1]) / 2.0f;
            }
        }
    }

    g_free (tmp);
    return TRUE;
}

static void
ufo_flatten_task_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
    UfoFlattenTaskPrivate *priv = UFO_FLATTEN_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_MODE:
            {
                Mode mode = string_to_mode (g_value_get_string (value));

                if (mode != M_0)
                    priv->mode = mode;
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_flatten_task_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
    UfoFlattenTaskPrivate *priv = UFO_FLATTEN_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_MODE:
            g_value_set_string (value, modes[priv->mode]);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_flatten_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_flatten_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_flatten_task_setup;
    iface->get_num_inputs = ufo_flatten_task_get_num_inputs;
    iface->get_num_dimensions = ufo_flatten_task_get_num_dimensions;
    iface->get_mode = ufo_flatten_task_get_mode;
    iface->get_requisition = ufo_flatten_task_get_requisition;
    iface->process = ufo_flatten_task_process;
}

static void
ufo_flatten_task_class_init (UfoFlattenTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_flatten_task_set_property;
    oclass->get_property = ufo_flatten_task_get_property;
    oclass->finalize = ufo_flatten_task_finalize;

    properties[PROP_MODE] =
        g_param_spec_string ("mode",
            "Mode (min, max, sum, median)",
            "Mode (min, max, sum, median)",
            "",
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoFlattenTaskPrivate));
}

static void
ufo_flatten_task_init(UfoFlattenTask *self)
{
    self->priv = UFO_FLATTEN_TASK_GET_PRIVATE(self);
    self->priv->mode = M_MEDIAN;
}
