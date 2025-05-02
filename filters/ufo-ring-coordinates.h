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
 *
 * Authored by: Alexandre Lewkowicz (lewkow_a@epita.fr)
 */

#ifndef UFO_RING_COORDINATES_H
#define UFO_RING_COORDINATES_H

struct _UfoRingCoordinate {
    float x;
    float y;
    float r;
    float contrast;
    float intensity;
};

struct _UfoRingCoordinatesStream {
    float nb_elt;
    struct _UfoRingCoordinate *coord;
};

typedef struct _UfoRingCoordinate UfoRingCoordinate;
typedef struct _UfoRingCoordinatesStream URCS;

#endif
