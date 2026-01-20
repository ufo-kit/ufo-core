/*
 * Copyright (C) 2011-2019 Karlsruhe Institute of Technology
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

/**
 * Set mask to 1 if value exceeds threshold. If sgn is -1, value has to be below
 * threshold, if it is 1 it has to be above threshold, if it is 0 absolute value
 * is compared.
 */
kernel void
set_above_threshold (global float *input,
                         global float *output,
                         const float threshold,
                         const int sgn)
{
    int index = get_global_id (1) * get_global_size (0) + get_global_id (0);
    float value = input[index];
    if (!sgn) {
        value = fabs (value) - threshold;
    } else {
        value = sgn * (value - threshold);
    }

    output[index] = value > 0 ? 1 : 0;
}

kernel void
set_to_ones (global float *values)
{
    int x = get_global_id (0) + 1;
    int y = get_global_id (1) + 1;
    int width = get_global_size (0) + 2;

    values[y * width + x] = 1;
}

kernel void
grow_region_above_threshold (global float *input,
                             global float *visited,
                             global float *output,
                             global int *counter,
                             const float threshold)
{
    int x = get_global_id (0) + 1;
    int y = get_global_id (1) + 1;
    int width = get_global_size (0) + 2;
    int offset = y * width;
    int index = offset + x;
    float value, diff = 0.0f, maximum = 0.0f;
    int dxy;

    if (visited[index]) {
        return;
    }
    value = input[index];

    for (dxy = -1; dxy < 2; dxy += 2) {
        /* Check x +/- 1 */
        if (visited[offset + x + dxy]) {
            diff = fabs (value - input[offset + x + dxy]);
            if (diff > maximum) {
                maximum = diff;
            }
        }
        /* Check y +/- 1 */
        if (visited[(y + dxy) * width + x]) {
            diff = fabs (value - input[(y + dxy) * width + x]);
            if (diff > maximum) {
                maximum = diff;
            }
        }
    }
    if (maximum > threshold) {
        output[index] = 1;
        atomic_inc (counter);
    }
}

kernel void
fill_holes (global int *input,
            global int *visited,
            global int *output,
            global int *counter)
{
    int x = get_global_id (0) + 1;
    int y = get_global_id (1) + 1;
    int width = get_global_size (0) + 2;
    int offset = y * width;
    int index = offset + x;

    if (!visited[index] || input[index]) {
        return;
    }

    if (!(visited[offset + x - 1] && visited[offset + x + 1] && visited[(y - 1) * width + x] && visited[(y + 1) * width + x])) {
        output[index] = 0;
        atomic_inc (counter);
    }
}

