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

#ifndef UFO_JPEG_WRITER_JPEG_H
#define UFO_JPEG_WRITER_JPEG_H

#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_JPEG_WRITER             (ufo_jpeg_writer_get_type())
#define UFO_JPEG_WRITER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_JPEG_WRITER, UfoJpegWriter))
#define UFO_IS_JPEG_WRITER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_JPEG_WRITER))
#define UFO_JPEG_WRITER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_JPEG_WRITER, UfoJpegWriterClass))
#define UFO_IS_JPEG_WRITER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_JPEG_WRITER))
#define UFO_JPEG_WRITER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_JPEG_WRITER, UfoJpegWriterClass))


typedef struct _UfoJpegWriter           UfoJpegWriter;
typedef struct _UfoJpegWriterClass      UfoJpegWriterClass;
typedef struct _UfoJpegWriterPrivate    UfoJpegWriterPrivate;

struct _UfoJpegWriter {
    GObject parent_instance;

    UfoJpegWriterPrivate *priv;
};

struct _UfoJpegWriterClass {
    GObjectClass parent_class;
};

UfoJpegWriter  *ufo_jpeg_writer_new         (void);
void            ufo_jpeg_writer_set_quality (UfoJpegWriter *writer, gint quality);
GType           ufo_jpeg_writer_get_type    (void);

G_END_DECLS

#endif
