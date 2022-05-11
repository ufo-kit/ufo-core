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

#ifndef UFO_ADDRESSING_H
#define UFO_ADDRESSING_H

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include <glib-object.h>

typedef enum {
    ADDRESS_NONE = CL_ADDRESS_NONE,
    ADDRESS_CLAMP_TO_EDGE = CL_ADDRESS_CLAMP_TO_EDGE,
    ADDRESS_CLAMP = CL_ADDRESS_CLAMP,
    ADDRESS_REPEAT = CL_ADDRESS_REPEAT,
    ADDRESS_MIRRORED_REPEAT = CL_ADDRESS_MIRRORED_REPEAT
} AddressingMode;

static GEnumValue addressing_values[] = {
    { ADDRESS_NONE,             "ADDRESS_NONE",             "none" },
    { ADDRESS_CLAMP_TO_EDGE,    "ADDRESS_CLAMP_TO_EDGE",    "clamp_to_edge" },
    { ADDRESS_CLAMP,            "ADDRESS_CLAMP",            "clamp" },
    { ADDRESS_REPEAT,           "ADDRESS_REPEAT",           "repeat" },
    { ADDRESS_MIRRORED_REPEAT,  "ADDRESS_MIRRORED_REPEAT",  "mirrored_repeat" },
    { 0, NULL, NULL}
};

#endif
