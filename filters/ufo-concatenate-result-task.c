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

#include <stdlib.h> /* realloc */
#include <string.h> /* memcpy */
#include <assert.h>

#include "ufo-concatenate-result-task.h"
#include "ufo-ring-coordinates.h"


struct _PivFileMetadata {
    char *piv_file_name;
    unsigned piv_file_idx;
};

typedef struct _PivFileMetadata PivFileMetadata;

struct _UfoConcatenateResultTaskPrivate {
    URCS **data;
    PivFileMetadata *metadata;
    unsigned alloc_size;
    unsigned idx;
    unsigned current_output_idx;
    unsigned max_count;
    unsigned ring_count;
    unsigned img_alloc_size;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoConcatenateResultTask, ufo_concatenate_result_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_CONCATENATE_RESULT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CONCATENATE_RESULT_TASK, UfoConcatenateResultTaskPrivate))

enum {
    PROP_0,
    PROP_MAX_COUNT,
    PROP_RING_COUNT,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_concatenate_result_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_CONCATENATE_RESULT_TASK, NULL));
}

static void
ufo_concatenate_result_task_setup (UfoTask *task,
                       UfoResources *resources,
                       GError **error)
{
}

static void
ufo_concatenate_result_task_get_requisition (UfoTask *task,
                                             UfoBuffer **inputs,
                                             UfoRequisition *requisition,
                                             GError **error)
{
    requisition->n_dims = 1;
    /* Output size varies according to number of inputs to merge, and data
     * whithin the data */
    /* I don't know the output size, I'll allocate it myself once I have
     * processed the data */
    requisition->dims[0] = 0;
}

static guint
ufo_concatenate_result_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_concatenate_result_task_get_num_dimensions (UfoTask *task,
                                             guint input)
{
    return 2;
}

static UfoTaskMode
ufo_concatenate_result_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_REDUCTOR | UFO_TASK_MODE_CPU;
}

static void
ufo_concatenate_get_metadata(UfoBuffer *src, const char **piv_file_name,
                             unsigned *piv_file_idx)
{
    GValue *value;

    value = ufo_buffer_get_metadata(src, "piv_file_name");
    *piv_file_name = g_value_get_string(value);

    value = ufo_buffer_get_metadata(src, "piv_file_idx");
    *piv_file_idx = g_value_get_uint(value);
}

static void
initialize_field(UfoConcatenateResultTaskPrivate *priv, const char *piv_file_name,
                 unsigned piv_file_idx)
{
    priv->idx = 0;
    priv->alloc_size = 16;
    priv->data[piv_file_idx] = g_malloc (16 * sizeof (UfoRingCoordinate) +
                                         sizeof (float));
    priv->data[piv_file_idx]->nb_elt = 0;
    priv->metadata[piv_file_idx].piv_file_idx = piv_file_idx;
    priv->metadata[piv_file_idx].piv_file_name = g_strdup (piv_file_name);
}

static void
increase_buffer_size(UfoConcatenateResultTaskPrivate *priv)
{
    priv->img_alloc_size *= 2;
    priv->data = g_realloc (priv->data, sizeof (URCS *) * priv->img_alloc_size);
    priv->metadata = g_realloc (priv->metadata, sizeof (PivFileMetadata) * priv->img_alloc_size);

    // Make sure that we inform that the newly allocated image buffer do not
    // have any data yet.
    for (unsigned i = priv->img_alloc_size / 2; i < priv->img_alloc_size; ++i) {
        priv->data[i] = NULL;
        priv->metadata[i].piv_file_idx = (unsigned) -1;
        priv->metadata[i].piv_file_name = NULL;
    }
}

static gboolean
ufo_concatenate_result_task_process (UfoTask *task,
                                     UfoBuffer **inputs,
                                     UfoBuffer *output,
                                     UfoRequisition *requisition)
{
    const char *piv_file_name;
    unsigned piv_file_idx;
    UfoConcatenateResultTaskPrivate *priv;

    priv = UFO_CONCATENATE_RESULT_TASK_GET_PRIVATE (task);
    ufo_concatenate_get_metadata (inputs[0], &piv_file_name, &piv_file_idx);
    assert(priv->ring_count && "Ring count must be set\n");
    // Check if buffer needs to be increased to be able to store more images.

    if (piv_file_idx >= priv->img_alloc_size)
        increase_buffer_size (priv);

    if (priv->data[piv_file_idx] == NULL)
        initialize_field (priv, piv_file_name, piv_file_idx);

    float *input = ufo_buffer_get_host_array (inputs[0], NULL);
    unsigned nb_coord = (unsigned) *input;
    UfoRingCoordinate *coord = (UfoRingCoordinate *) (input + 1);

    if (nb_coord > priv->max_count) {
        g_print("Concatenate: max_count : Ignoring radius %f. %u rings found, "
                "maximum is %u\n", coord[0].r, nb_coord, priv->max_count);
        return TRUE;
    }

    if (priv->idx + nb_coord > priv->alloc_size) {
        priv->alloc_size += nb_coord;
        priv->data[piv_file_idx] = g_realloc (priv->data[piv_file_idx], sizeof (UfoRingCoordinate) * priv->alloc_size + sizeof (float));
    }

    for (unsigned i = 0; i < nb_coord; ++i)
        priv->data[piv_file_idx]->coord[priv->idx++] = coord[i];

    priv->data[piv_file_idx]->nb_elt += (float) nb_coord;

    return TRUE;
}

static unsigned
find_next_idx_for_output (UfoConcatenateResultTaskPrivate *priv)
{
    unsigned res = priv->current_output_idx;

    while (res < priv->img_alloc_size && priv->data[res] == NULL)
        ++res;

    return res;
}

static void
attach_output_metadata (UfoBuffer *output, PivFileMetadata *meta)
{
    GValue value_uint = G_VALUE_INIT;
    GValue value_string = G_VALUE_INIT;

    g_value_init (&value_uint, G_TYPE_UINT);
    g_value_set_uint (&value_uint, meta->piv_file_idx);
    ufo_buffer_set_metadata (output, "piv_file_idx", &value_uint);

    g_value_init (&value_string, G_TYPE_STRING);
    g_value_set_string (&value_string, meta->piv_file_name);
    ufo_buffer_set_metadata(output, "piv_file_name", &value_string);
}

static gboolean
ufo_concatenate_result_task_generate (UfoTask *task,
                                      UfoBuffer *output,
                                      UfoRequisition *requisition)
{
    UfoConcatenateResultTaskPrivate *priv = UFO_CONCATENATE_RESULT_TASK_GET_PRIVATE (task);
    priv->current_output_idx = find_next_idx_for_output (priv);

    if (priv->current_output_idx >= priv->img_alloc_size)
        return FALSE;

    unsigned ring_idx = priv->current_output_idx;
    attach_output_metadata (output, &priv->metadata[ring_idx]);
    URCS *rings = priv->data[ring_idx];
    /* There will be at most idx elements */
    UfoRequisition new_req = {
        .dims[0] = 1 + (unsigned) rings->nb_elt * sizeof (UfoRingCoordinate) / sizeof (float),
        .n_dims = 1,
    };
    ufo_buffer_resize (output, &new_req);
    float *res = ufo_buffer_get_host_array (output, NULL);
    memcpy (res, rings, (unsigned) (rings->nb_elt) * sizeof (UfoRingCoordinate) + sizeof (float));
    ++priv->current_output_idx;
    return TRUE;
}

static void
ufo_concatenate_result_task_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    UfoConcatenateResultTaskPrivate *priv = UFO_CONCATENATE_RESULT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_MAX_COUNT:
            priv->max_count = g_value_get_uint (value);
            break;

        case PROP_RING_COUNT:
            if (priv->data != NULL)
                g_free (priv->data);

            if (priv->metadata != NULL)
                g_free (priv->metadata);

            priv->ring_count = g_value_get_uint (value);
            priv->data = g_malloc (sizeof (URCS *) * priv->ring_count);
            priv->metadata = g_malloc (sizeof (PivFileMetadata) * priv->ring_count);
            priv->img_alloc_size = priv->ring_count;

            for (unsigned i = 0; i < priv->ring_count; ++i) {
                priv->data[i] = NULL;
                priv->metadata[i].piv_file_idx = (unsigned) -1;
                priv->metadata[i].piv_file_name = NULL;
            }

            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_concatenate_result_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoConcatenateResultTaskPrivate *priv = UFO_CONCATENATE_RESULT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_MAX_COUNT:
            g_value_set_uint (value, priv->max_count);
            break;

        case PROP_RING_COUNT:
            g_value_set_uint (value, priv->ring_count);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_concatenate_result_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_concatenate_result_task_parent_class)->finalize (object);
    UfoConcatenateResultTaskPrivate *priv = UFO_CONCATENATE_RESULT_TASK_GET_PRIVATE (object);

    for (unsigned i = 0; i < priv->img_alloc_size; ++i) {
        if (priv->data[i])
            g_free (priv->data[i]);

        if (priv->metadata[i].piv_file_name)
            g_free (priv->metadata[i].piv_file_name);
    }

    g_free (priv->data);
    g_free (priv->metadata);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_concatenate_result_task_setup;
    iface->get_num_inputs = ufo_concatenate_result_task_get_num_inputs;
    iface->get_num_dimensions = ufo_concatenate_result_task_get_num_dimensions;
    iface->get_mode = ufo_concatenate_result_task_get_mode;
    iface->get_requisition = ufo_concatenate_result_task_get_requisition;
    iface->process = ufo_concatenate_result_task_process;
    iface->generate = ufo_concatenate_result_task_generate;
}

static void
ufo_concatenate_result_task_class_init (UfoConcatenateResultTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_concatenate_result_task_set_property;
    gobject_class->get_property = ufo_concatenate_result_task_get_property;
    gobject_class->finalize = ufo_concatenate_result_task_finalize;

    properties[PROP_MAX_COUNT] =
        g_param_spec_uint ("max-count",
            "The maximum number of rings desired per ring pattern",
            "The maximum number of rings desired per ring pattern",
            1, G_MAXUINT, 60,
            G_PARAM_READWRITE);

    properties[PROP_RING_COUNT] =
        g_param_spec_uint ("ring-count",
            "The number of ring pattern generated per image",
            "The maximum number of rings desired per ring pattern",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoConcatenateResultTaskPrivate));
}

static void
ufo_concatenate_result_task_init(UfoConcatenateResultTask *self)
{
    self->priv = UFO_CONCATENATE_RESULT_TASK_GET_PRIVATE(self);
    self->priv->idx = 0;
    self->priv->alloc_size = 0;
    self->priv->data = NULL;
    self->priv->metadata = NULL;
    self->priv->max_count = 60;
    self->priv->ring_count = 0;
    self->priv->img_alloc_size = 0;
    self->priv->current_output_idx = 0;
}
