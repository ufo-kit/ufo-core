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

#include "ufo-reader.h"

typedef UfoReaderIface UfoReaderInterface;

G_DEFINE_INTERFACE (UfoReader, ufo_reader, 0)


gboolean
ufo_reader_can_open (UfoReader *reader,
                     const gchar *filename)
{
    return UFO_READER_GET_IFACE (reader)->can_open (reader, filename);
}

gboolean
ufo_reader_open (UfoReader *reader,
                 const gchar *filename,
                 guint start,
                 GError **error)
{
    return UFO_READER_GET_IFACE (reader)->open (reader, filename, start, error);
}

void
ufo_reader_close (UfoReader *reader)
{
    UFO_READER_GET_IFACE (reader)->close (reader);
}

gboolean
ufo_reader_data_available (UfoReader *reader)
{
    return UFO_READER_GET_IFACE (reader)->data_available (reader);
}

gboolean
ufo_reader_get_meta (UfoReader *reader,
                     UfoRequisition *requisition,
                     gsize *num_images,
                     UfoBufferDepth *bitdepth,
                     GError **error)
{
    return UFO_READER_GET_IFACE (reader)->get_meta (reader, requisition, num_images, bitdepth, error);
}

gsize
ufo_reader_read (UfoReader *reader,
                 UfoBuffer *buffer,
                 UfoRequisition *requisition,
                 guint roi_y,
                 guint roi_height,
                 guint roi_step,
                 guint image_step)
{
    return UFO_READER_GET_IFACE (reader)->read (reader, buffer, requisition, roi_y, roi_height, roi_step, image_step);
}

static void
ufo_reader_default_init (UfoReaderInterface *iface)
{
}
