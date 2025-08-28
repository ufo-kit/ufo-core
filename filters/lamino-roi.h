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

#ifndef LAMINO_ROI_H
#define LAMINO_ROI_H

#define EXTRACT_INT(region, index) g_value_get_int (g_value_array_get_nth ((region), (index)))

#include <glib-object.h>

G_BEGIN_DECLS

void clip (gint result[2], gfloat extrema[2], gint maximum);
void determine_x_extrema (gfloat extrema[2], GValueArray *x_extrema, GValueArray *y_extrema,
                          gfloat tomo_angle, gfloat x_center);
void determine_y_extrema (gfloat extrema[2], GValueArray *x_extrema, GValueArray *y_extrema,
                          gfloat z_extrema[2], gfloat tomo_angle, gfloat lamino_angle,
                          gfloat y_center);
void determine_x_region (gint result[2], GValueArray *x_extrema, GValueArray *y_extrema, gfloat tomo_angle,
                         gfloat x_center, gint width);
void determine_y_region (gint result[2], GValueArray *x_extrema, GValueArray *y_extrema, gfloat z_extrema[2],
                         gfloat tomo_angle, gfloat lamino_angle, gfloat y_center, gint height);

G_END_DECLS

#endif
