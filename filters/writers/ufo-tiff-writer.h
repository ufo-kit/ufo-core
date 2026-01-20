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

#ifndef UFO_TIFF_WRITER_TIFF_H
#define UFO_TIFF_WRITER_TIFF_H

#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_TIFF_WRITER             (ufo_tiff_writer_get_type())
#define UFO_TIFF_WRITER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_TIFF_WRITER, UfoTiffWriter))
#define UFO_IS_TIFF_WRITER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_TIFF_WRITER))
#define UFO_TIFF_WRITER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_TIFF_WRITER, UfoTiffWriterClass))
#define UFO_IS_TIFF_WRITER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_TIFF_WRITER))
#define UFO_TIFF_WRITER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_TIFF_WRITER, UfoTiffWriterClass))


typedef struct _UfoTiffWriter           UfoTiffWriter;
typedef struct _UfoTiffWriterClass      UfoTiffWriterClass;
typedef struct _UfoTiffWriterPrivate    UfoTiffWriterPrivate;

struct _UfoTiffWriter {
    GObject parent_instance;

    UfoTiffWriterPrivate *priv;
};

struct _UfoTiffWriterClass {
    GObjectClass parent_class;
};

UfoTiffWriter  *ufo_tiff_writer_new       (void);
GType           ufo_tiff_writer_get_type  (void);

G_END_DECLS

#endif
