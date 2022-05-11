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

#include <math.h>
#include <glib.h>
#include "ufo-math.h"

UfoPoint *
ufo_point_new (gdouble x, gdouble y, gdouble z)
{
    UfoPoint *point = g_new (UfoPoint, 1);
    point->x = x;
    point->y = y;
    point->z = z;

    return point;
}

void
ufo_point_free (UfoPoint *point)
{
    g_free (point);
}

void
ufo_point_mul_scalar (UfoPoint *point, gdouble value)
{
    point->x *= value;
    point->y *= value;
    point->z *= value;
}

void
ufo_point_add (UfoPoint *point, UfoPoint *other)
{
    point->x += other->x;
    point->y += other->y;
    point->z += other->z;
}

void
ufo_point_subtract (UfoPoint *point, UfoPoint *other)
{
    point->x -= other->x;
    point->y -= other->y;
    point->z -= other->z;
}

gdouble
ufo_point_dot_product (UfoPoint *point, UfoPoint *other)
{
    return point->x * other->x + point->y * other->y + point->z * other->z;
}

void
ufo_point_rotate_x (UfoPoint *point, gdouble angle)
{
    UfoPoint tmp;
    gdouble sin_angle = sin (angle);
    gdouble cos_angle = cos (angle);

    tmp.x = point->x;
    tmp.y = point->y * cos_angle - point->z * sin_angle;
    tmp.z = point->y * sin_angle + point->z * cos_angle;
    *point = tmp;
}

void
ufo_point_rotate_y (UfoPoint *point, gdouble angle)
{
    UfoPoint tmp;
    gdouble sin_angle = sin (angle);
    gdouble cos_angle = cos (angle);

    tmp.x = point->x * cos_angle + point->z * sin_angle;
    tmp.y = point->y;
    tmp.z = -point->x * sin_angle + point->z * cos_angle;
    *point = tmp;
}

void
ufo_point_rotate_z (UfoPoint *point, gdouble angle)
{

    UfoPoint tmp;
    gdouble sin_angle = sin (angle);
    gdouble cos_angle = cos (angle);

    tmp.x = point->x * cos_angle - point->y * sin_angle;
    tmp.y = point->x * sin_angle + point->y * cos_angle;
    tmp.z = point->z;
    *point = tmp;
}

gdouble
ufo_clip_value (gdouble value, gdouble minimum, gdouble maximum)
{
    return MAX (minimum, MIN (maximum, value));
}

static gdouble
find_extremum (gdouble *array, gint num_values, gint sgn)
{
    gint i;
    gdouble minimum = sgn * array[0];

    for (i = 1; i < num_values; i++) {
        if (sgn * array[i] < minimum) {
            minimum = sgn * array[i];
        }
    }

    return sgn * minimum;
}

gdouble
ufo_array_maximum (gdouble *array, gint num_values)
{
    return find_extremum (array, num_values, -1);
}

gdouble
ufo_array_minimum (gdouble *array, gint num_values)
{
    return find_extremum (array, num_values, 1);
}

gsize
ufo_math_compute_closest_smaller_power_of_2 (gsize value)
{
    gdouble integer;
    modf (log2 (value), &integer);

    return (gsize) pow (2, integer);
}
