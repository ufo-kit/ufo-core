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

#ifndef UFO_TIFF_READER_TIFF_H
#define UFO_TIFF_READER_TIFF_H

#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_TIFF_READER             (ufo_tiff_reader_get_type())
#define UFO_TIFF_READER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_TIFF_READER, UfoTiffReader))
#define UFO_IS_TIFF_READER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_TIFF_READER))
#define UFO_TIFF_READER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_TIFF_READER, UfoTiffReaderClass))
#define UFO_IS_TIFF_READER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_TIFF_READER))
#define UFO_TIFF_READER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_TIFF_READER, UfoTiffReaderClass))


typedef struct _UfoTiffReader           UfoTiffReader;
typedef struct _UfoTiffReaderClass      UfoTiffReaderClass;
typedef struct _UfoTiffReaderPrivate    UfoTiffReaderPrivate;

struct _UfoTiffReader {
    GObject parent_instance;

    UfoTiffReaderPrivate *priv;
};

struct _UfoTiffReaderClass {
    GObjectClass parent_class;
};

UfoTiffReader  *ufo_tiff_reader_new       (void);
GType           ufo_tiff_reader_get_type  (void);

G_END_DECLS

#endif
