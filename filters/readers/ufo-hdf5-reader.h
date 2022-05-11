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

#ifndef UFO_HDF5_READER_HDF5_H
#define UFO_HDF5_READER_HDF5_H

#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_HDF5_READER             (ufo_hdf5_reader_get_type())
#define UFO_HDF5_READER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_HDF5_READER, UfoHdf5Reader))
#define UFO_IS_HDF5_READER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_HDF5_READER))
#define UFO_HDF5_READER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_HDF5_READER, UfoHdf5ReaderClass))
#define UFO_IS_HDF5_READER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_HDF5_READER))
#define UFO_HDF5_READER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_HDF5_READER, UfoHdf5ReaderClass))


typedef struct _UfoHdf5Reader           UfoHdf5Reader;
typedef struct _UfoHdf5ReaderClass      UfoHdf5ReaderClass;
typedef struct _UfoHdf5ReaderPrivate    UfoHdf5ReaderPrivate;

struct _UfoHdf5Reader {
    GObject parent_instance;

    UfoHdf5ReaderPrivate *priv;
};

struct _UfoHdf5ReaderClass {
    GObjectClass parent_class;
};

UfoHdf5Reader  *ufo_hdf5_reader_new       (void);
GType           ufo_hdf5_reader_get_type  (void);

G_END_DECLS

#endif
