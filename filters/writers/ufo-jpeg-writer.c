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

#include <stdio.h>
#include <jpeglib.h>
#include <jerror.h>

#include "writers/ufo-writer.h"
#include "writers/ufo-jpeg-writer.h"


struct _UfoJpegWriterPrivate {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr error;
    FILE *fp;
    int quality;
};

static void ufo_writer_interface_init (UfoWriterIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoJpegWriter, ufo_jpeg_writer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_WRITER,
                                                ufo_writer_interface_init))

#define UFO_JPEG_WRITER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_JPEG_WRITER, UfoJpegWriterPrivate))

UfoJpegWriter *
ufo_jpeg_writer_new (void)
{
    UfoJpegWriter *writer = g_object_new (UFO_TYPE_JPEG_WRITER, NULL);
    return writer;
}

void
ufo_jpeg_writer_set_quality (UfoJpegWriter *writer, gint quality)
{
    writer->priv->quality = quality;
}

static gboolean
ufo_jpeg_writer_can_open (UfoWriter *writer,
                          const gchar *filename)
{
    return g_str_has_suffix (filename, ".jpg") || g_str_has_suffix (filename, ".jpeg");
}

static void
ufo_jpeg_writer_open (UfoWriter *writer,
                      const gchar *filename)
{
    UfoJpegWriterPrivate *priv;

    priv = UFO_JPEG_WRITER_GET_PRIVATE (writer);
    priv->fp = fopen (filename, "wb");
}

static void
ufo_jpeg_writer_close (UfoWriter *writer)
{
    UfoJpegWriterPrivate *priv;

    priv = UFO_JPEG_WRITER_GET_PRIVATE (writer);
    g_assert (priv->fp != NULL);
    fclose (priv->fp);
    priv->fp = NULL;
}

static void
ufo_jpeg_writer_write (UfoWriter *writer,
                       UfoWriterImage *image)
{
    UfoJpegWriterPrivate *priv;
    gint row_stride;
    gboolean is_rgb;

    is_rgb = image->requisition->n_dims == 3 && image->requisition->dims[2] == 3;

    priv = UFO_JPEG_WRITER_GET_PRIVATE (writer);
    priv->cinfo.image_width = image->requisition->dims[0];
    priv->cinfo.image_height = image->requisition->dims[1];
    priv->cinfo.input_components = is_rgb ? 3 : 1;
    priv->cinfo.in_color_space = is_rgb ? JCS_RGB : JCS_GRAYSCALE;

    /*
     * We have to ignore the given bit depth for JPEG. Note that this way, we
     * might convert data twice because the parent ufo_writer_write already
     * converts to the given bit depth.
     */

    if (image->depth != UFO_BUFFER_DEPTH_8U) {
        image->depth = UFO_BUFFER_DEPTH_8U;
        ufo_writer_convert_inplace (image);
    }

    jpeg_stdio_dest (&priv->cinfo, priv->fp);
    jpeg_set_defaults (&priv->cinfo);
    jpeg_set_quality (&priv->cinfo, priv->quality, 1);
    jpeg_start_compress (&priv->cinfo, TRUE);

    row_stride = (gint) image->requisition->dims[0] * priv->cinfo.input_components;

    while (priv->cinfo.next_scanline < priv->cinfo.image_height) {
        JSAMPROW row_pointer[1];
        row_pointer[0] = (JSAMPROW) (((gchar *) image->data) + priv->cinfo.next_scanline * row_stride);
        jpeg_write_scanlines (&priv->cinfo, row_pointer, 1);
    }

    jpeg_finish_compress (&priv->cinfo);
    jpeg_abort_compress (&priv->cinfo);
}

static void
ufo_jpeg_writer_finalize (GObject *object)
{
    UfoJpegWriterPrivate *priv;

    priv = UFO_JPEG_WRITER_GET_PRIVATE (object);

    jpeg_destroy_compress (&priv->cinfo);

    if (priv->fp != NULL)
        ufo_jpeg_writer_close (UFO_WRITER (object));

    G_OBJECT_CLASS (ufo_jpeg_writer_parent_class)->finalize (object);
}

static void
ufo_writer_interface_init (UfoWriterIface *iface)
{
    iface->can_open = ufo_jpeg_writer_can_open;
    iface->open = ufo_jpeg_writer_open;
    iface->close = ufo_jpeg_writer_close;
    iface->write = ufo_jpeg_writer_write;
}

static void
ufo_jpeg_writer_class_init(UfoJpegWriterClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = ufo_jpeg_writer_finalize;

    g_type_class_add_private (gobject_class, sizeof (UfoJpegWriterPrivate));
}

static void
ufo_jpeg_writer_init (UfoJpegWriter *self)
{
    UfoJpegWriterPrivate *priv = NULL;

    self->priv = priv = UFO_JPEG_WRITER_GET_PRIVATE (self);
    priv->fp = NULL;
    priv->quality = 95;
    priv->cinfo.err = jpeg_std_error (&priv->error);
    jpeg_create_compress (&priv->cinfo);
}
