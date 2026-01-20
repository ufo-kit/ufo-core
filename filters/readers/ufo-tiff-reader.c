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

#include <tiffio.h>

#include "readers/ufo-reader.h"
#include "readers/ufo-tiff-reader.h"


struct _UfoTiffReaderPrivate {
    TIFF    *tiff;
    gboolean more;
    gsize num_images;
};

static void ufo_reader_interface_init (UfoReaderIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoTiffReader, ufo_tiff_reader, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_READER,
                                                ufo_reader_interface_init))

#define UFO_TIFF_READER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_TIFF_READER, UfoTiffReaderPrivate))

UfoTiffReader *
ufo_tiff_reader_new (void)
{
    UfoTiffReader *reader = g_object_new (UFO_TYPE_TIFF_READER, NULL);
    return reader;
}

static gboolean
ufo_tiff_reader_can_open (UfoReader *reader,
                         const gchar *filename)
{
    return g_str_has_suffix (filename, ".tiff") || g_str_has_suffix (filename, ".tif");
}

static gboolean
ufo_tiff_reader_open (UfoReader *reader,
                      const gchar *filename,
                      guint start,
                      GError **error)
{
    UfoTiffReaderPrivate *priv;

    priv = UFO_TIFF_READER_GET_PRIVATE (reader);
    priv->num_images = 0;
    priv->tiff = TIFFOpen (filename, "r");
    priv->more = FALSE;

    if (priv->tiff == NULL) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "Cannot open %s", filename);
        return FALSE;
    }

	do {
	    priv->num_images++;
	} while (TIFFReadDirectory(priv->tiff));

    if (start < priv->num_images) {
        priv->more = TRUE;
        if (TIFFSetDirectory (priv->tiff, start) != 1) {
            g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                         "Cannot find first image in %s", filename);
            return FALSE;
        }
    }

    return TRUE;
}

static void
ufo_tiff_reader_close (UfoReader *reader)
{
    UfoTiffReaderPrivate *priv;

    priv = UFO_TIFF_READER_GET_PRIVATE (reader);
    g_assert (priv->tiff != NULL);
    TIFFClose (priv->tiff);
    priv->tiff = NULL;
}

static gboolean
ufo_tiff_reader_data_available (UfoReader *reader)
{
    UfoTiffReaderPrivate *priv;

    priv = UFO_TIFF_READER_GET_PRIVATE (reader);

    return priv->more && priv->tiff != NULL;
}

static void
read_data (UfoTiffReaderPrivate *priv,
           UfoBuffer *buffer,
           UfoRequisition *requisition,
           guint16 bits,
           guint roi_y,
           guint roi_height,
           guint roi_step)
{
    gchar *dst;
    gsize step;
    gsize offset;

    step = requisition->dims[0] * bits / 8;
    dst = (gchar *) ufo_buffer_get_host_array (buffer, NULL);
    offset = 0;

    if (requisition->n_dims == 3) {
        /* RGB data */
        gchar *src;
        gsize plane_size;

        /* Allow things like roi_height=1 and roi_step=20 */
        plane_size = step * ((roi_height - 1) / roi_step + 1);
        src = g_new0 (gchar, step * 3);

        for (guint i = roi_y; i < roi_y + roi_height; i += roi_step) {
            guint xd = 0;
            guint xs = 0;

            TIFFReadScanline (priv->tiff, src, i, 0);

            for (; xd < requisition->dims[0]; xd += 1, xs += 3) {
                dst[offset + xd] = src[xs];
                dst[offset + plane_size + xd] = src[xs + 1];
                dst[offset + 2 * plane_size + xd] = src[xs + 2];
            }

            offset += step;
        }

        g_free (src);
    }
    else {
        for (guint i = roi_y; i < roi_y + roi_height; i += roi_step) {
            TIFFReadScanline (priv->tiff, dst + offset, i, 0);
            offset += step;
        }
    }
}

static void
read_64_bit_data (UfoTiffReaderPrivate *priv,
                  UfoBuffer *buffer,
                  UfoRequisition *requisition,
                  guint roi_y,
                  guint roi_height,
                  guint roi_step)
{
    gdouble *src;
    gfloat *dst;

    dst = ufo_buffer_get_host_array (buffer, NULL);
    src = g_new0 (gdouble, requisition->dims[0]);

    for (guint i = roi_y; i < roi_y + roi_height; i += roi_step) {
        TIFFReadScanline (priv->tiff, src, i, 0);

        for (guint j = 0; j < requisition->dims[0]; j++)
            dst[j] = (gfloat) src[j];

        dst += requisition->dims[0];
    }

    g_free (src);
}

static gsize
ufo_tiff_reader_read (UfoReader *reader,
                      UfoBuffer *buffer,
                      UfoRequisition *requisition,
                      guint roi_y,
                      guint roi_height,
                      guint roi_step,
                      guint image_step)
{
    UfoTiffReaderPrivate *priv;
    guint16 bits;
    gsize num_read = 0;

    priv = UFO_TIFF_READER_GET_PRIVATE (reader);

    TIFFGetField (priv->tiff, TIFFTAG_BITSPERSAMPLE, &bits);

    if (bits == 64)
        read_64_bit_data (priv, buffer, requisition, roi_y, roi_height, roi_step);
    else
        read_data (priv, buffer, requisition, bits, roi_y, roi_height, roi_step);

    do {
        priv->more = TIFFReadDirectory (priv->tiff) == 1;
        num_read++;
        if (!priv->more) {
            break;
        }
    } while (num_read < image_step);

    return num_read;
}

static gboolean
ufo_tiff_reader_get_meta (UfoReader *reader,
                          UfoRequisition *requisition,
                          gsize *num_images,
                          UfoBufferDepth *bitdepth,
                          GError **error)
{
    UfoTiffReaderPrivate *priv;
    guint32 width;
    guint32 height;
    guint32 samples;
    guint16 bits_per_sample;

    priv = UFO_TIFF_READER_GET_PRIVATE (reader);
    g_assert (priv->tiff != NULL);

    TIFFGetField (priv->tiff, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField (priv->tiff, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetField (priv->tiff, TIFFTAG_SAMPLESPERPIXEL, &samples);
    TIFFGetField (priv->tiff, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);

    requisition->n_dims = samples == 3 ? 3 : 2;
    requisition->dims[0] = (gsize) width;
    requisition->dims[1] = (gsize) height;
    requisition->dims[2] = samples == 3 ? 3 : 0;
    *num_images = priv->num_images;

    switch (bits_per_sample) {
        case 8:
            *bitdepth = UFO_BUFFER_DEPTH_8U;
            break;
        case 12:
            *bitdepth = UFO_BUFFER_DEPTH_12U;
            break;
        case 16:
            *bitdepth = UFO_BUFFER_DEPTH_16U;
            break;
        default:
            *bitdepth = UFO_BUFFER_DEPTH_32F;
    }

    return TRUE;
}

static void
ufo_tiff_reader_finalize (GObject *object)
{
    UfoTiffReaderPrivate *priv;

    priv = UFO_TIFF_READER_GET_PRIVATE (object);

    if (priv->tiff != NULL)
        ufo_tiff_reader_close (UFO_READER (object));

    G_OBJECT_CLASS (ufo_tiff_reader_parent_class)->finalize (object);
}

static void
ufo_reader_interface_init (UfoReaderIface *iface)
{
    iface->can_open = ufo_tiff_reader_can_open;
    iface->open = ufo_tiff_reader_open;
    iface->close = ufo_tiff_reader_close;
    iface->read = ufo_tiff_reader_read;
    iface->get_meta = ufo_tiff_reader_get_meta;
    iface->data_available = ufo_tiff_reader_data_available;
}

static void
ufo_tiff_reader_class_init(UfoTiffReaderClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = ufo_tiff_reader_finalize;

    g_type_class_add_private (gobject_class, sizeof (UfoTiffReaderPrivate));
}

static void
ufo_tiff_reader_init (UfoTiffReader *self)
{
    UfoTiffReaderPrivate *priv = NULL;

    self->priv = priv = UFO_TIFF_READER_GET_PRIVATE (self);
    priv->tiff = NULL;
    priv->more = FALSE;
    TIFFSetWarningHandler(NULL);
}
