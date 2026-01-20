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

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ufo-ringwriter-task.h"
#include "ufo-ring-coordinates.h"

struct _UfoRingwriterTaskPrivate {
    char *filename;
    unsigned scale;
    FILE *file;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoRingwriterTask, ufo_ringwriter_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_RINGWRITER_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_RINGWRITER_TASK, UfoRingwriterTaskPrivate))

enum {
    PROP_0,
    PROP_PATH,
    PROP_SCALE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_ringwriter_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_RINGWRITER_TASK, NULL));
}

static void
ufo_ringwriter_task_setup (UfoTask *task,
                       UfoResources *resources,
                       GError **error)
{
}

static void
ufo_ringwriter_task_get_requisition (UfoTask *task,
                                     UfoBuffer **inputs,
                                     UfoRequisition *requisition,
                                     GError **error)
{
    requisition->n_dims = 0;
}

static guint
ufo_ringwriter_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_ringwriter_task_get_num_dimensions (UfoTask *task,
                                        guint input)
{
    return 1;
}

static UfoTaskMode
ufo_ringwriter_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_SINK | UFO_TASK_MODE_CPU;
}

static void
get_file_metadata (UfoBuffer *src, const char **piv_file_name, unsigned *piv_file_idx)
{
    GValue *value;

    value = ufo_buffer_get_metadata(src, "piv_file_name");
    *piv_file_name = g_value_get_string(value);

    value = ufo_buffer_get_metadata(src, "piv_file_idx");
    *piv_file_idx = g_value_get_uint(value);
}

static void
ufo_ringwriter_write_metadata (UfoBuffer *src, FILE *file)
{
    const char *piv_file_name;
    unsigned piv_file_idx;
    char str[256];
    int count;

    get_file_metadata (src, &piv_file_name, &piv_file_idx);
    count = sprintf(str, "filename %s\n", piv_file_name);

    if (count < 0)
        g_print ("Unable to convert data to output text file\n");

    fwrite (str, sizeof (char), (size_t) count, file);
    count = sprintf(str, "index %u\n", piv_file_idx);

    if (count < 0)
        g_print ("Unable to convert data to output text file\n");

    fwrite (str, sizeof (char), (size_t) count, file);
}

static gboolean
ufo_ringwriter_task_process (UfoTask *task,
                             UfoBuffer **inputs,
                             UfoBuffer *output,
                             UfoRequisition *requisition)
{
    UfoRingwriterTaskPrivate *priv = UFO_RINGWRITER_TASK_GET_PRIVATE (task);

    if (priv->file == NULL) {
        static int file_count = 0;
        char filename[256];
        sprintf(filename, "%s%i.txt", priv->filename, file_count);
        priv->file = fopen (filename, "w");
        ++file_count;
    }

    URCS *ring_stream = (URCS *) ufo_buffer_get_host_array (inputs[0], NULL);

    FILE *file = priv->file;
    ufo_ringwriter_write_metadata(inputs[0], file);
    char str[256];
    int count = sprintf(str, "ring_count %u\n", (unsigned) ring_stream->nb_elt);

    if (count < 0)
        g_print ("Unable to convert data to output text file\n");

    fwrite (str, sizeof (char), (size_t) count, file);

    for (unsigned i = 0; i < ring_stream->nb_elt; ++i) {
        int x = (int) roundf (ring_stream->coord[i].x * (float) priv->scale);
        int y = (int) roundf (ring_stream->coord[i].y * (float) priv->scale);
        float r =  ring_stream->coord[i].r * (float) priv->scale;
        count = sprintf(str, "ring_coord %i %i %f\n", x, y, r);
        if (count < 0)
            g_print ("Unable to convert data to output text file\n");
        size_t write_count = 0;
        char *tmp = str;

        while (write_count != (size_t) count) {
            size_t c = fwrite (tmp, sizeof (char), (size_t) count, file);
            if (c == 0)
                g_print ("Unable to write data to output text file\n");
            tmp += c;
            write_count += c;
        }
    }
    return TRUE;
}

static void
ufo_ringwriter_task_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
    UfoRingwriterTaskPrivate *priv = UFO_RINGWRITER_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_SCALE:
            priv->scale = g_value_get_uint(value);
            break;
        case PROP_PATH:
            priv->filename = g_strdup (g_value_get_string (value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_ringwriter_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoRingwriterTaskPrivate *priv = UFO_RINGWRITER_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_SCALE:
            g_value_set_uint (value, priv->scale);
            break;
        case PROP_PATH:
            g_value_set_string (value, priv->filename);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_ringwriter_task_finalize (GObject *object)
{
    UfoRingwriterTaskPrivate *priv = UFO_RINGWRITER_TASK_GET_PRIVATE (object);

    if (priv->file)
        fclose (priv->file);

    if (priv->filename)
        g_free (priv->filename);

    G_OBJECT_CLASS (ufo_ringwriter_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_ringwriter_task_setup;
    iface->get_num_inputs = ufo_ringwriter_task_get_num_inputs;
    iface->get_num_dimensions = ufo_ringwriter_task_get_num_dimensions;
    iface->get_mode = ufo_ringwriter_task_get_mode;
    iface->get_requisition = ufo_ringwriter_task_get_requisition;
    iface->process = ufo_ringwriter_task_process;
}

static void
ufo_ringwriter_task_class_init (UfoRingwriterTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_ringwriter_task_set_property;
    oclass->get_property = ufo_ringwriter_task_get_property;
    oclass->finalize = ufo_ringwriter_task_finalize;

    properties[PROP_SCALE] =
        g_param_spec_uint ("scale",
            "Says by how much rings should be increased",
            "Says by how much rings should be increased",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    properties[PROP_PATH] =
        g_param_spec_string ("filename",
            "Path for the output file to write data",
            "Path for the output file to write data",
            "results",
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoRingwriterTaskPrivate));
}

static void
ufo_ringwriter_task_init(UfoRingwriterTask *self)
{
    self->priv = UFO_RINGWRITER_TASK_GET_PRIVATE(self);
    self->priv->filename = g_strdup ("results");
    self->priv->file = NULL;
    self->priv->scale = 1;
}
