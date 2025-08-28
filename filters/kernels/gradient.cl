/*
 * Copyright (C) 2011-2018 Karlsruhe Institute of Technology
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

float compute_horizontal (global float *input,
                          int idx,
                          int idy,
                          int width) {
    int dx = 1;
    if (idx == width - 1) {
        dx = 0;
    } else if (idx == 0) {
        dx = 2;
    }

    return 0.5f * (input[idy * width + idx + dx] - input[idy * width + idx + dx - 2]);
}

float compute_vertical (global float *input,
                        int idx,
                        int idy,
                        int width,
                        int height) {
    int dy = 1;
    if (idy == height - 1) {
        dy = 0;
    } else if (idy == 0) {
        dy = 2;
    }

    return 0.5f * (input[(idy + dy) * width + idx] - input[(idy + dy - 2) * width + idx]);
}

kernel void horizontal (global float *input,
                        global float *output)
{
    int width = get_global_size (0);
    int idx = get_global_id (0);
    int idy = get_global_id (1);

    output[idy * width + idx] = compute_horizontal (input, idx, idy, width);
}

kernel void vertical (global float *input,
                      global float *output)
{
    int width = get_global_size (0);
    int height = get_global_size (1);
    int idx = get_global_id (0);
    int idy = get_global_id (1);

    output[idy * width + idx] = compute_vertical (input, idx, idy, width, height);
}

kernel void both (global float *input,
                  global float *output)
{
    int width = get_global_size (0);
    int height = get_global_size (1);
    int idx = get_global_id (0);
    int idy = get_global_id (1);

    output[idy * width + idx] = compute_vertical (input, idx, idy, width, height) +
                                compute_horizontal (input, idx, idy, width);
}

kernel void both_abs (global float *input,
                      global float *output)
{
    int width = get_global_size (0);
    int height = get_global_size (1);
    int idx = get_global_id (0);
    int idy = get_global_id (1);

    output[idy * width + idx] = fabs (compute_vertical (input, idx, idy, width, height)) +
                                fabs (compute_horizontal (input, idx, idy, width));
}
