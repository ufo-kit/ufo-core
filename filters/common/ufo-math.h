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

#ifndef UFO_MATH_H
#define UFO_MATH_H

#include <glib.h>

#define UFO_MATH_EPSILON 1e-7
#define UFO_MATH_ARE_ALMOST_EQUAL(a, b) (ABS ((a) - (b)) < UFO_MATH_EPSILON)
#define UFO_MATH_NUM_CHUNKS(n, k) (((n) - 1) / (k) + 1)


typedef struct {
    gdouble x, y, z;
} UfoPoint;

UfoPoint        *ufo_point_new                  (gdouble      x,
                                                 gdouble      y,
                                                 gdouble      z);
void             ufo_point_free                 (UfoPoint    *point);
void             ufo_point_mul_scalar           (UfoPoint    *point,
                                                 gdouble      value);
void             ufo_point_add                  (UfoPoint    *point,
                                                 UfoPoint    *other);
void             ufo_point_subtract             (UfoPoint    *point,
                                                 UfoPoint    *other);
gdouble          ufo_point_dot_product          (UfoPoint    *point,
                                                 UfoPoint    *other);
void             ufo_point_rotate_x             (UfoPoint    *point,
                                                 gdouble      angle);
void             ufo_point_rotate_y             (UfoPoint    *point,
                                                 gdouble      angle);
void             ufo_point_rotate_z             (UfoPoint    *point,
                                                 gdouble      angle);
gdouble          ufo_array_maximum              (gdouble     *array,
                                                 gint         num_values);
gdouble          ufo_array_minimum              (gdouble     *array,
                                                 gint         num_values);
gdouble          ufo_clip_value                 (gdouble value,
                                                 gdouble minimum,
                                                 gdouble maximum);
gsize            ufo_math_compute_closest_smaller_power_of_2 (gsize value);

#endif
