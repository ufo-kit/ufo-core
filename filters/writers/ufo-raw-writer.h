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

#ifndef UFO_RAW_WRITER_RAW_H
#define UFO_RAW_WRITER_RAW_H

#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_RAW_WRITER             (ufo_raw_writer_get_type())
#define UFO_RAW_WRITER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_RAW_WRITER, UfoRawWriter))
#define UFO_IS_RAW_WRITER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_RAW_WRITER))
#define UFO_RAW_WRITER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_RAW_WRITER, UfoRawWriterClass))
#define UFO_IS_RAW_WRITER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_RAW_WRITER))
#define UFO_RAW_WRITER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_RAW_WRITER, UfoRawWriterClass))


typedef struct _UfoRawWriter           UfoRawWriter;
typedef struct _UfoRawWriterClass      UfoRawWriterClass;
typedef struct _UfoRawWriterPrivate    UfoRawWriterPrivate;

struct _UfoRawWriter {
    GObject parent_instance;

    UfoRawWriterPrivate *priv;
};

struct _UfoRawWriterClass {
    GObjectClass parent_class;
};

UfoRawWriter  *ufo_raw_writer_new       (void);
GType          ufo_raw_writer_get_type  (void);

G_END_DECLS

#endif
