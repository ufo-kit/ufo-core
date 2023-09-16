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

#include <stdio.h>
#include <stdlib.h>

#include "readers/ufo-reader.h"
#include "readers/ufo-edf-reader.h"


struct _UfoEdfReaderPrivate {
    FILE *fp;
    guint start;
    gssize size;
    gsize height;
    guint bytes_per_sample;
    gboolean big_endian;
};

static void ufo_reader_interface_init (UfoReaderIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoEdfReader, ufo_edf_reader, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_READER,
                                                ufo_reader_interface_init))

#define UFO_EDF_READER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_EDF_READER, UfoEdfReaderPrivate))

UfoEdfReader *
ufo_edf_reader_new (void)
{
    UfoEdfReader *reader = g_object_new (UFO_TYPE_EDF_READER, NULL);
    return reader;
}

static gboolean
ufo_edf_reader_can_open (UfoReader *reader,
                         const gchar *filename)
{
    return g_str_has_suffix (filename, ".edf");
}

static gboolean
ufo_edf_reader_open (UfoReader *reader,
                     const gchar *filename,
                     guint start,
                     GError **error)
{
    UfoEdfReaderPrivate *priv;

    priv = UFO_EDF_READER_GET_PRIVATE (reader);
    priv->fp = fopen (filename, "rb");

    fseek (priv->fp, 0L, SEEK_END);
    priv->size = (gsize) ftell (priv->fp);
    fseek (priv->fp, 0L, SEEK_SET);
    priv->start = start;

    return TRUE;
}

static void
ufo_edf_reader_close (UfoReader *reader)
{
    UfoEdfReaderPrivate *priv;

    priv = UFO_EDF_READER_GET_PRIVATE (reader);
    g_assert (priv->fp != NULL);
    fclose (priv->fp);
    priv->fp = NULL;
    priv->size = 0;
}

static gboolean
ufo_edf_reader_data_available (UfoReader *reader)
{
    UfoEdfReaderPrivate *priv;

    priv = UFO_EDF_READER_GET_PRIVATE (reader);
    return priv->fp != NULL && ftell (priv->fp) < priv->size;
}

static gsize
ufo_edf_reader_read (UfoReader *reader,
                     UfoBuffer *buffer,
                     UfoRequisition *requisition,
                     guint roi_y,
                     guint roi_height,
                     guint roi_step,
                     guint image_step)
{
    UfoEdfReaderPrivate *priv;
    gsize num_bytes;
    gsize num_read;
    gssize offset;
    gchar *data;
    gsize to_skip;
    guint start = 0;

    priv = UFO_EDF_READER_GET_PRIVATE (reader);
    data = (gchar *) ufo_buffer_get_host_array (buffer, NULL);

    /* size of the image width in bytes */
    const gsize width = requisition->dims[0] * priv->bytes_per_sample;
    const guint num_rows = requisition->dims[1];
    if (priv->start) {
        start = priv->start;
        priv->start = 0;
    }
    const gsize end_position = ftell (priv->fp) + priv->height * width;

    offset = 0;

    /* Go to the first desired row at *start* image index */
    fseek (priv->fp, start * priv->height * width + roi_y * width, SEEK_CUR);

    if (roi_step == 1) {
        /* Read the full ROI at once if no stepping is specified */
        num_bytes = width * roi_height;
        num_read = fread (data, 1, num_bytes, priv->fp);

        if (num_read != num_bytes)
            return 0;
    }
    else {
        for (guint i = 0; i < num_rows - 1; i++) {
            num_read = fread (data + offset, 1, width, priv->fp);

            if (num_read != width)
                return 0;

            offset += width;
            fseek (priv->fp, (roi_step - 1) * width, SEEK_CUR);
        }

        /* Read the last row without moving the file pointer so that the fseek to
         * the image end works properly */
        num_read = fread (data + offset, 1, width, priv->fp);

        if (num_read != width)
            return 0;
    }

    /* Go to the image end to be in a consistent state for the next read */
    fseek (priv->fp, end_position, SEEK_SET);

    /* Skip the desired number of images */
    to_skip = MIN (image_step - 1, (priv->size - (gsize) ftell (priv->fp)) / (priv->height * width));
    fseek (priv->fp, to_skip * priv->height * width, SEEK_CUR);

    if ((G_BYTE_ORDER == G_LITTLE_ENDIAN) && priv->big_endian) {
        guint32 *conv = (guint32 *) ufo_buffer_get_host_array (buffer, NULL);
        guint n_pixels = requisition->dims[0] * requisition->dims[1];

        for (guint i = 0; i < n_pixels; i++)
            conv[i] = g_ntohl (conv[i]);
    }

    return to_skip + 1;
}

static void
ufo_edf_reader_get_depth (const gchar *value, UfoBufferDepth *depth, guint *bytes)
{
    struct {
        const gchar *name;
        UfoBufferDepth depth;
        guint bytes;
    }
    map[] = {
        {"UnsignedShort",   UFO_BUFFER_DEPTH_16U, 2},
        {"SignedInteger",   UFO_BUFFER_DEPTH_32S, 4},
        {"UnsignedLong",    UFO_BUFFER_DEPTH_32U, 4},
        {"Float",           UFO_BUFFER_DEPTH_32F, 4},
        {"FloatValue",      UFO_BUFFER_DEPTH_32F, 4},
        {NULL}
    };

    for (guint i = 0; map[i].name != NULL; i++) {
        if (!g_strcmp0 (value, map[i].name)) {
            *depth = map[i].depth;
            *bytes = map[i].bytes;
            return;
        }
    }

    g_warning ("Unsupported data type");
    *depth = UFO_BUFFER_DEPTH_8U;
    *bytes = 1;
}

static gboolean
ufo_edf_reader_get_meta (UfoReader *reader,
                         UfoRequisition *requisition,
                         gsize *num_images,
                         UfoBufferDepth *bitdepth,
                         GError **error)
{
    UfoEdfReaderPrivate *priv;
    gchar **tokens;
    gchar *header, *header_trig_position;
    gsize data_position;

    priv = UFO_EDF_READER_GET_PRIVATE (reader);
    header = g_malloc (priv->size);

    if (fread (header, 1, priv->size, priv->fp) != (gsize) priv->size) {
        g_set_error_literal (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                             "Could not read EDF header.");
        g_free (header);
        fclose (priv->fp);
        priv->fp = NULL;
        return FALSE;
    }

    header_trig_position = g_strstr_len (header, -1, "}");
    data_position = header_trig_position - header + 2;

    if (header_trig_position == NULL || data_position % 512) {
        g_set_error_literal (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                             "Corrupt EDF header or not an EDF file.");
        g_free (header);
        fclose (priv->fp);
        priv->fp = NULL;
        return FALSE;
    }

    /* Go to the data position */
    fseek (priv->fp, data_position, SEEK_SET);
    /* Don't process binary data */
    header[data_position] = '\0';

    tokens = g_strsplit (header, ";", 0);
    priv->big_endian = FALSE;
    requisition->n_dims = 2;

    for (guint i = 0; tokens[i] != NULL; i++) {
        gchar **key_value;
        gchar *key;
        gchar *value;

        key_value = g_strsplit (tokens[i], "=", 0);

        if (key_value[0] == NULL || key_value[1] == NULL)
            continue;

        key = g_strstrip (key_value[0]);
        value = g_strstrip (key_value[1]);

        if (!g_strcmp0 (key, "Dim_1")) {
            requisition->dims[0] = (guint) atoi (value);
        }
        else if (!g_strcmp0 (key, "Dim_2")) {
            requisition->dims[1] = priv->height = atoi (value);
        }
        else if (!g_strcmp0 (key, "DataType")) {
            ufo_edf_reader_get_depth (value, bitdepth, &priv->bytes_per_sample);
        }
        else if (!g_strcmp0 (key, "ByteOrder") && !g_strcmp0 (value, "HighByteFirst")) {
            priv->big_endian = TRUE;
        }
        else if (!g_strcmp0 (key, "Size")) {
            /*
             * Override file size if Size key is given. Using the determined
             * file size can cause wrong assumption about the number of images
             * in the EDF file.
             */
            priv->size = atoi (value);
        }

        g_strfreev (key_value);
    }

    *num_images = requisition->dims[0] * requisition->dims[1] * priv->bytes_per_sample / (priv->size - data_position);
    g_strfreev (tokens);
    g_free (header);
    return TRUE;
}

static void
ufo_edf_reader_finalize (GObject *object)
{
    UfoEdfReaderPrivate *priv;

    priv = UFO_EDF_READER_GET_PRIVATE (object);

    if (priv->fp != NULL) {
        fclose (priv->fp);
        priv->fp = NULL;
    }

    G_OBJECT_CLASS (ufo_edf_reader_parent_class)->finalize (object);
}

static void
ufo_reader_interface_init (UfoReaderIface *iface)
{
    iface->can_open = ufo_edf_reader_can_open;
    iface->open = ufo_edf_reader_open;
    iface->close = ufo_edf_reader_close;
    iface->read = ufo_edf_reader_read;
    iface->get_meta = ufo_edf_reader_get_meta;
    iface->data_available = ufo_edf_reader_data_available;
}

static void
ufo_edf_reader_class_init (UfoEdfReaderClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = ufo_edf_reader_finalize;

    g_type_class_add_private (gobject_class, sizeof (UfoEdfReaderPrivate));
}

static void
ufo_edf_reader_init (UfoEdfReader *self)
{
    UfoEdfReaderPrivate *priv = NULL;

    self->priv = priv = UFO_EDF_READER_GET_PRIVATE (self);
    priv->fp = NULL;
}
