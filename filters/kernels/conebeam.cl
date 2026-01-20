/*
 * Copyright (C) 2011-2017 Karlsruhe Institute of Technology
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

kernel void weight_projection (global float *projection,
                               global float *result,
                               const float2 center,
                               const float source_distance,
                               const float overall_distance,
                               const float magnification_recip,
                               const float cos_theta)
{
    int idx = get_global_id (0);
    int idy = get_global_id (1);
    int index = idy * get_global_size (0) + idx;
    float x = (((float) idx) - center.x + 0.5f) * magnification_recip;
    float y = (((float) idy) - center.y + 0.5f) * magnification_recip;

    /* Based on eq. 28 from "Direct cone beam SPECT reconstruction with camera tilt" */
    result[index] = projection[index] * source_distance * cos_theta / sqrt (overall_distance * overall_distance + x * x + y * y);
}
