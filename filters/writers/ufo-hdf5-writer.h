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

#ifndef UFO_HDF5_WRITER_HDF5_H
#define UFO_HDF5_WRITER_HDF5_H

#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_HDF5_WRITER             (ufo_hdf5_writer_get_type())
#define UFO_HDF5_WRITER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_HDF5_WRITER, UfoHdf5Writer))
#define UFO_IS_HDF5_WRITER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_HDF5_WRITER))
#define UFO_HDF5_WRITER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_HDF5_WRITER, UfoHdf5WriterClass))
#define UFO_IS_HDF5_WRITER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_HDF5_WRITER))
#define UFO_HDF5_WRITER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_HDF5_WRITER, UfoHdf5WriterClass))


typedef struct _UfoHdf5Writer           UfoHdf5Writer;
typedef struct _UfoHdf5WriterClass      UfoHdf5WriterClass;
typedef struct _UfoHdf5WriterPrivate    UfoHdf5WriterPrivate;

struct _UfoHdf5Writer {
    GObject parent_instance;

    UfoHdf5WriterPrivate *priv;
};

struct _UfoHdf5WriterClass {
    GObjectClass parent_class;
};

UfoHdf5Writer  *ufo_hdf5_writer_new       (void);
GType           ufo_hdf5_writer_get_type  (void);

G_END_DECLS

#endif
