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

#include "writers/ufo-writer.h"
#include "writers/ufo-raw-writer.h"


struct _UfoRawWriterPrivate {
    FILE *fp;
};

static void ufo_writer_interface_init (UfoWriterIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoRawWriter, ufo_raw_writer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_WRITER,
                                                ufo_writer_interface_init))

#define UFO_RAW_WRITER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_RAW_WRITER, UfoRawWriterPrivate))

UfoRawWriter *
ufo_raw_writer_new (void)
{
    UfoRawWriter *writer = g_object_new (UFO_TYPE_RAW_WRITER, NULL);
    return writer;
}

static gboolean
ufo_raw_writer_can_open (UfoWriter *writer,
                         const gchar *filename)
{
    return g_str_has_suffix (filename, ".raw");
}

static void
ufo_raw_writer_open (UfoWriter *writer,
                     const gchar *filename)
{
    UfoRawWriterPrivate *priv;
    
    priv = UFO_RAW_WRITER_GET_PRIVATE (writer);
    priv->fp = filename == NULL ? stdout : fopen (filename, "wb");
}

static void
ufo_raw_writer_close (UfoWriter *writer)
{
    UfoRawWriterPrivate *priv;
    
    priv = UFO_RAW_WRITER_GET_PRIVATE (writer);
    g_assert (priv->fp != NULL);
    fclose (priv->fp);
    priv->fp = NULL;
}

static gsize
bytes_per_pixel (UfoBufferDepth depth)
{
    switch (depth) {
        case UFO_BUFFER_DEPTH_8U:
            return 1;
        case UFO_BUFFER_DEPTH_16U:
        case UFO_BUFFER_DEPTH_16S:
            return 2;
        default:
            return 4;
    }
}

static void
ufo_raw_writer_write (UfoWriter *writer,
                      UfoWriterImage *image)
{
    UfoRawWriterPrivate *priv;
    gsize size = bytes_per_pixel (image->depth);

    priv = UFO_RAW_WRITER_GET_PRIVATE (writer);

    for (guint i = 0; i < image->requisition->n_dims; i++)
        size *= image->requisition->dims[i];

    fwrite (image->data, 1, size, priv->fp);
}

static void
ufo_raw_writer_finalize (GObject *object)
{
    UfoRawWriterPrivate *priv;
    
    priv = UFO_RAW_WRITER_GET_PRIVATE (object);

    if (priv->fp != NULL)
        ufo_raw_writer_close (UFO_WRITER (object));

    G_OBJECT_CLASS (ufo_raw_writer_parent_class)->finalize (object);
}

static void
ufo_writer_interface_init (UfoWriterIface *iface)
{
    iface->can_open = ufo_raw_writer_can_open;
    iface->open = ufo_raw_writer_open;
    iface->close = ufo_raw_writer_close;
    iface->write = ufo_raw_writer_write;
}

static void
ufo_raw_writer_class_init(UfoRawWriterClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = ufo_raw_writer_finalize;

    g_type_class_add_private (gobject_class, sizeof (UfoRawWriterPrivate));
}

static void
ufo_raw_writer_init (UfoRawWriter *self)
{
    UfoRawWriterPrivate *priv = NULL;

    self->priv = priv = UFO_RAW_WRITER_GET_PRIVATE (self);
    priv->fp = NULL;
}
