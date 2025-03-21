/*
 * Copyright (C) 2017 Karlsruhe Institute of Technology
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

#ifndef UFO_CTGEOMETRY_H
#define UFO_CTGEOMETRY_H

#include <ufo/ufo.h>
#include "ufo-scarray.h"
#include "ufo-conebeam.h"

typedef struct {
    UfoScarray *x;
    UfoScarray *y;
    UfoScarray *z;
} UfoScpoint;

typedef struct {
    UfoScpoint *position;
    UfoScpoint *angle;
} UfoScvector;

typedef struct {
    UfoScpoint  *source_position;
    UfoScpoint  *volume_angle;
    UfoScvector *axis;
    UfoScvector *detector;
} UfoCTGeometry;

UfoScpoint        *ufo_scpoint_new                               (UfoScarray *x,
                                                                  UfoScarray *y,
                                                                  UfoScarray *z);
UfoScpoint        *ufo_scpoint_copy                              (const UfoScpoint *point);
void               ufo_scpoint_free                              (UfoScpoint *point);
gboolean           ufo_scpoint_are_almost_zero                   (UfoScpoint *point);
UfoScvector       *ufo_scvector_new                              (UfoScpoint *position,
                                                                  UfoScpoint *angle);
void               ufo_scvector_free                             (UfoScvector *vector);
UfoCTGeometry     *ufo_ctgeometry_new                            (void);
void               ufo_ctgeometry_free                           (UfoCTGeometry *geometry);

#endif
