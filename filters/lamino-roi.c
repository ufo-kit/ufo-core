/*
 * Copyright (C) 2016 Karlsruhe Institute of Technology
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
#include "lamino-roi.h"


static inline void
swap (gint *first, gint *second) {
    gint tmp;

    tmp = *first;
    *first = *second;
    *second = tmp;
}

/**
 * Determine the left and right column to read from a projection at a given
 * tomographic angle.
 */
void
determine_x_extrema (gfloat extrema[2], GValueArray *x_extrema, GValueArray *y_extrema,
                     gfloat tomo_angle, gfloat x_center)
{
    gfloat sin_tomo, cos_tomo;
    gint x_min, x_max, y_min, y_max;

    sin_tomo = sin (tomo_angle);
    cos_tomo = cos (tomo_angle);
    x_min = EXTRACT_INT (x_extrema, 0);
    /* The interval is right-opened when OpenCL indices for both x and y are generated, */
    /* so the last index doesn't count */
    x_max = EXTRACT_INT (x_extrema, 1) - 1;
    y_min = EXTRACT_INT (y_extrema, 0);
    y_max = EXTRACT_INT (y_extrema, 1) - 1;

    if (sin_tomo < 0) {
        swap (&y_min, &y_max);
    }
    if (cos_tomo < 0) {
        swap (&x_min, &x_max);
    }

    /* -1 to make sure the interpolation doesn't reach to uninitialized values*/
    extrema[0] = cos_tomo * x_min + sin_tomo * y_min + x_center - 1;
    /* +1 because extrema[1] will be accessed by interpolation
     * but the region in copying is right-open */
    extrema[1] = cos_tomo * x_max + sin_tomo * y_max + x_center + 1;
}

/**
 * Determine the top and bottom row to read from a projection at given
 * tomographic and laminographic angles.
 */
void
determine_y_extrema (gfloat extrema[2], GValueArray *x_extrema, GValueArray *y_extrema,
                     gfloat z_extrema[2], gfloat tomo_angle, gfloat lamino_angle,
                     gfloat y_center)
{
    gfloat sin_tomo, cos_tomo, sin_lamino, cos_lamino;
    gint x_min, x_max, y_min, y_max;

    sin_tomo = sin (tomo_angle);
    cos_tomo = cos (tomo_angle);
    sin_lamino = sin (lamino_angle);
    cos_lamino = cos (lamino_angle);
    x_min = EXTRACT_INT (x_extrema, 0);
    x_max = EXTRACT_INT (x_extrema, 1) - 1;
    y_min = EXTRACT_INT (y_extrema, 0);
    y_max = EXTRACT_INT (y_extrema, 1) - 1;

    if (sin_tomo < 0) {
        swap (&x_min, &x_max);
    }
    if (cos_tomo > 0) {
        swap (&y_min, &y_max);
    }

    extrema[0] = sin_tomo * x_min - cos_tomo * y_min;
    extrema[1] = sin_tomo * x_max - cos_tomo * y_max;
    extrema[0] = extrema[0] * cos_lamino + z_extrema[0] * sin_lamino + y_center - 1;
    extrema[1] = extrema[1] * cos_lamino + z_extrema[1] * sin_lamino + y_center + 1;
}

/**
 * clip:
 * @result: resulting clipped extrema
 * @extrema: (min, max)
 * @maximum: projection width or height
 *
 * Clip extrema to an allowed interval [0, projection width/height)
 */
void
clip (gint result[2], gfloat extrema[2], gint maximum)
{
    result[0] = (gint) floorf (extrema[0]);
    result[1] = (gint) ceilf (extrema[1]);

    if (result[0] < 0) {
        result[0] = 0;
    }
    if (result[0] > maximum) {
        result[0] = maximum;
    }
    if (result[1] < 0) {
        result[1] = 0;
    }
    if (result[1] > maximum) {
        result[1] = maximum;
    }

    if (result[0] == result[1]) {
        if (result[1] < maximum) {
            result[1]++;
        } else if (result[0] > 0) {
            result[0]--;
        } else {
            g_warning ("Cannot extend");
        }
    } else if (result[0] > result[1]) {
        g_warning ("Invalid extrema: minimum larger than maximum");
    }
}

/**
 * Determine the left and right column to read from a projection at a given
 * tomographic angle. The result is bound to [0, projection width)
 */
void
determine_x_region (gint result[2], GValueArray *x_extrema, GValueArray *y_extrema, gfloat tomo_angle,
                    gfloat x_center, gint width)
{
    gfloat extrema[2];

    determine_x_extrema (extrema, x_extrema, y_extrema, tomo_angle, x_center);

    clip (result, extrema, width);
}

/**
 * Determine the top and bottom column to read from a projection at given
 * tomographic and laminographic angles. The result is bound to
 * [0, projection height).
 */
void
determine_y_region (gint result[2], GValueArray *x_extrema, GValueArray *y_extrema, gfloat z_extrema[2],
                    gfloat tomo_angle, gfloat lamino_angle, gfloat y_center, gint height)
{
    gfloat extrema[2];

    determine_y_extrema (extrema, x_extrema, y_extrema, z_extrema, tomo_angle,
                         lamino_angle, y_center);
    clip (result, extrema, height);
}

