/*
 * Copyright (C) 2015-2017 Karlsruhe Institute of Technology
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

#ifndef UFO_INTERPOLATION_H
#define UFO_INTERPOLATION_H

#include <glib-object.h>

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

typedef enum {
    INTERPOLATE_NEAREST = CL_FILTER_NEAREST,
    INTERPOLATE_LINEAR = CL_FILTER_LINEAR,
} Interpolation;

static GEnumValue interpolation_values[] = {
    { INTERPOLATE_NEAREST, "INTERPOLATE_NEAREST", "nearest" },
    { INTERPOLATE_LINEAR,  "INTERPOLATE_LINEAR",  "linear" },
    { 0, NULL, NULL}
};

#endif
