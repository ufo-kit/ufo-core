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

#include <string.h>
#include "ufo-buffer-task.h"

/**
 * SECTION:ufo-buffer-task
 * @Short_description: Buffer input in memory
 * @Title: buffer
 *
 * Read input data until stream ends into a local memory buffer. After that
 * output the stream again.
 */

struct _UfoMetaData
{
    GValue *value;
    char *name;
};

typedef struct _UfoMetaData UfoMetadata;

struct _UfoArray
{
    guint nb_elt;
    // Array of nb_elt
    UfoMetadata data[1];
};

typedef struct _UfoArray UfoArray;

struct _UfoBufferTaskPrivate {
    guchar *data;
    UfoArray **metadata;
    guint n_prealloc;
    gsize n_elements;
    gsize current_element;
    gsize size;
    gsize current_size;
    gsize meta_current_size;
    gsize dup_count;
    gsize loop;
    gsize dup_current;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoBufferTask, ufo_buffer_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_BUFFER_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BUFFER_TASK, UfoBufferTaskPrivate))

enum {
    PROP_0,
    PROP_NUM_PREALLOC,
    PROP_DUP_COUNT,
    PROP_LOOP,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_buffer_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_BUFFER_TASK, NULL));
}

static void
ufo_buffer_task_setup (UfoTask *task,
                       UfoResources *resources,
                       GError **error)
{
}

static void
ufo_buffer_task_get_requisition (UfoTask *task,
                                 UfoBuffer **inputs,
                                 UfoRequisition *requisition,
                                 GError **error)
{
    UfoBufferTaskPrivate *priv;

    priv = UFO_BUFFER_TASK_GET_PRIVATE (task);
    priv->size = ufo_buffer_get_size (inputs[0]);
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_buffer_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_buffer_task_get_num_dimensions (UfoTask *task,
                               guint input)
{
    g_return_val_if_fail (input == 0, 0);
    return 2;
}

static UfoTaskMode
ufo_buffer_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_REDUCTOR | UFO_TASK_MODE_CPU;
}

static void
ufo_buffer_task_copy_metadata_out (UfoTask *task, UfoBuffer *output)
{
    UfoBufferTaskPrivate *priv;
    UfoArray *meta;

    priv = UFO_BUFFER_TASK_GET_PRIVATE (task);

    meta = priv->metadata[priv->current_element];

    for (unsigned i = 0; i < meta->nb_elt; ++i) {
        ufo_buffer_set_metadata (output, meta->data[i].name, meta->data[i].value);
    }
}

static void
ufo_buffer_task_copy_metadata_in (UfoTask *task, UfoBuffer *input)
{
    UfoBufferTaskPrivate *priv;
    UfoArray *meta;
    unsigned idx = 0;
    GValue *new;
    GValue *value;
    GList *names;
    GList *it;

    priv = UFO_BUFFER_TASK_GET_PRIVATE (task);

    names = ufo_buffer_get_metadata_keys (input);
    guint length = g_list_length (names);
    meta = g_malloc0 (sizeof (UfoMetadata) * length + sizeof (UfoArray));
    meta->nb_elt = length;
    for (it = names; it; it = it->next) {
        value = ufo_buffer_get_metadata (input, it->data);
        new = g_malloc0 (sizeof (GValue));
        g_value_init (new, G_VALUE_TYPE ((GValue *) value));
        g_value_copy (value, new);
        meta->data[idx].value = new;
        meta->data[idx].name = g_strdup (it->data);
        ++idx;
    }
    priv->metadata[priv->n_elements] = meta;
}

static gboolean
ufo_buffer_task_process (UfoTask *task,
                         UfoBuffer **inputs,
                         UfoBuffer *output,
                         UfoRequisition *requisition)
{
    UfoBufferTaskPrivate *priv;

    priv = UFO_BUFFER_TASK_GET_PRIVATE (task);

    if (priv->data == NULL) {
        priv->current_size = priv->n_prealloc * priv->size;
        priv->data = g_malloc0 (priv->current_size);
    }
    if (priv->metadata == NULL) {
        priv->meta_current_size = priv->n_prealloc * sizeof (UfoMetadata *);
        priv->metadata = g_malloc0 (priv->current_size);
    }

    if (priv->current_size <= priv->n_elements * priv->size) {
        priv->current_size *= 2;
        priv->data = g_realloc (priv->data, priv->current_size);
    }
    if (priv->meta_current_size <= priv->n_elements * sizeof (UfoMetadata *)) {
        priv->meta_current_size *= 2;
        priv->metadata = g_realloc (priv->metadata, priv->meta_current_size);
    }

    g_memmove (priv->data + priv->n_elements * priv->size,
               ufo_buffer_get_host_array (inputs[0], NULL),
               priv->size);

    ufo_buffer_task_copy_metadata_in (task, inputs[0]);

    priv->n_elements++;
    return TRUE;
}

static gboolean
ufo_buffer_task_generate (UfoTask *task,
                          UfoBuffer *output,
                          UfoRequisition *requisition)
{
    UfoBufferTaskPrivate *priv;

    priv = UFO_BUFFER_TASK_GET_PRIVATE (task);

    if (priv->loop) {
        if (priv->current_element == priv->n_elements) {
            --priv->dup_count;
            priv->current_element = 0;
        }
        if (!priv->dup_count)
            return FALSE;
    }
    else if (priv->current_element == priv->n_elements)
        return FALSE;

    g_memmove (ufo_buffer_get_host_array (output, NULL),
               priv->data + priv->current_element * priv->size,
               priv->size);
    ufo_buffer_task_copy_metadata_out (task, output);

    if (priv->loop)
        priv->current_element++;
    else if (priv->dup_count == priv->dup_current) {
        priv->current_element++;
        priv->dup_current = 1;
    }
    else
        ++priv->dup_current;
    return TRUE;
}

static void
ufo_buffer_task_finalize (GObject *object)
{
    UfoBufferTaskPrivate *priv;

    priv = UFO_BUFFER_TASK_GET_PRIVATE (object);

    if (priv->data != NULL) {
        g_free (priv->data);
        priv->data = NULL;
    }
    if (priv->metadata) {
        for (unsigned i = 0; i < priv->n_elements; ++i) {
            UfoArray *metadata = priv->metadata[i];
            for (unsigned j = 0; j < metadata->nb_elt; ++j) {
                g_free (metadata->data[j].value);
                metadata->data[j].value = NULL;
                g_free (metadata->data[j].name);
                metadata->data[j].name = NULL;
            }
            g_free (priv->metadata[i]);
            priv->metadata[i] = NULL;
        }
        g_free (priv->metadata);
        priv->metadata = NULL;
    }

    G_OBJECT_CLASS (ufo_buffer_task_parent_class)->finalize (object);
}

static void
ufo_buffer_task_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    UfoBufferTaskPrivate *priv;

    priv = UFO_BUFFER_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_PREALLOC:
            priv->n_prealloc = (guint) g_value_get_uint (value);
            break;
        case PROP_DUP_COUNT:
            priv->dup_count = (guint) g_value_get_uint (value);
            break;
        case PROP_LOOP:
            priv->loop = (gboolean) g_value_get_boolean (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_buffer_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoBufferTaskPrivate *priv;

    priv = UFO_BUFFER_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_PREALLOC:
            g_value_set_uint (value, priv->n_prealloc);
            break;
        case PROP_DUP_COUNT:
            g_value_set_uint (value, priv->dup_count);
            break;
        case PROP_LOOP:
            g_value_set_boolean (value, priv->loop);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_buffer_task_setup;
    iface->get_num_inputs = ufo_buffer_task_get_num_inputs;
    iface->get_num_dimensions = ufo_buffer_task_get_num_dimensions;
    iface->get_mode = ufo_buffer_task_get_mode;
    iface->get_requisition = ufo_buffer_task_get_requisition;
    iface->process = ufo_buffer_task_process;
    iface->generate = ufo_buffer_task_generate;
}

static void
ufo_buffer_task_class_init (UfoBufferTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = ufo_buffer_task_finalize;
    oclass->set_property = ufo_buffer_task_set_property;
    oclass->get_property = ufo_buffer_task_get_property;

    properties[PROP_NUM_PREALLOC] =
        g_param_spec_uint ("number",
            "Number of pre-allocated \"pages\"",
            "Number of pre-allocated \"pages\"",
            1, 32768, 4,
            G_PARAM_READWRITE);

    properties[PROP_DUP_COUNT] =
        g_param_spec_uint ("dup-count",
            "Number of times each image should be duplicated",
            "Number of times each image should be duplicated",
            1, 32768, 1,
            G_PARAM_READWRITE);

    properties[PROP_LOOP] =
        g_param_spec_boolean ("loop",
            "Duplicated the data in a loop manner dup-count times",
            "Duplicated the data in a loop manner dup-count times",
            0,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoBufferTaskPrivate));
}

static void
ufo_buffer_task_init(UfoBufferTask *self)
{
    self->priv = UFO_BUFFER_TASK_GET_PRIVATE(self);
    self->priv->data = NULL;
    self->priv->n_prealloc = 4;
    self->priv->n_elements = 0;
    self->priv->current_element = 0;
    self->priv->dup_count = 1;
    self->priv->loop = 0;
    self->priv->dup_current = 1;
}
