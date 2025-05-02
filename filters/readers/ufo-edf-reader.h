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

#ifndef UFO_EDF_READER_EDF_H
#define UFO_EDF_READER_EDF_H

#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_EDF_READER             (ufo_edf_reader_get_type())
#define UFO_EDF_READER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_EDF_READER, UfoEdfReader))
#define UFO_IS_EDF_READER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_EDF_READER))
#define UFO_EDF_READER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_EDF_READER, UfoEdfReaderClass))
#define UFO_IS_EDF_READER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_EDF_READER))
#define UFO_EDF_READER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_EDF_READER, UfoEdfReaderClass))


typedef struct _UfoEdfReader           UfoEdfReader;
typedef struct _UfoEdfReaderClass      UfoEdfReaderClass;
typedef struct _UfoEdfReaderPrivate    UfoEdfReaderPrivate;

struct _UfoEdfReader {
    GObject parent_instance;

    UfoEdfReaderPrivate *priv;
};

struct _UfoEdfReaderClass {
    GObjectClass parent_class;
};

UfoEdfReader  *ufo_edf_reader_new       (void);
GType          ufo_edf_reader_get_type  (void);

G_END_DECLS

#endif
