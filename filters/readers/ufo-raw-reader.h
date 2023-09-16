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

#ifndef UFO_RAW_READER_RAW_H
#define UFO_RAW_READER_RAW_H

#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_RAW_READER             (ufo_raw_reader_get_type())
#define UFO_RAW_READER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_RAW_READER, UfoRawReader))
#define UFO_IS_RAW_READER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_RAW_READER))
#define UFO_RAW_READER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_RAW_READER, UfoRawReaderClass))
#define UFO_IS_RAW_READER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_RAW_READER))
#define UFO_RAW_READER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_RAW_READER, UfoRawReaderClass))


typedef struct _UfoRawReader           UfoRawReader;
typedef struct _UfoRawReaderClass      UfoRawReaderClass;
typedef struct _UfoRawReaderPrivate    UfoRawReaderPrivate;

struct _UfoRawReader {
    GObject parent_instance;

    UfoRawReaderPrivate *priv;
};

struct _UfoRawReaderClass {
    GObjectClass parent_class;
};

UfoRawReader  *ufo_raw_reader_new       (void);
GType          ufo_raw_reader_get_type  (void);

G_END_DECLS

#endif
