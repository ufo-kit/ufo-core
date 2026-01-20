/*
 * Copyright (C) 2015-2016 Karlsruhe Institute of Technology
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

#ifndef UFO_HDF5_H
#define UFO_HDF5_H

#undef H5_USE_16_API

#define H5Dopen_vers    2
#define H5Dcreate_vers  2
#define H5Gopen_vers    2
#define H5Gcreate_vers  2

#include <glib.h>
#include <hdf5.h>

gboolean ufo_hdf5_can_open (const gchar *filename);

#endif
