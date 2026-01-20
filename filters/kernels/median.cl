/*
 * Copyright (C) 2014-2016 Karlsruhe Institute of Technology
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

/* These kernels assume that MEDIAN_BOX_SIZE is defined! */
#ifndef MEDIAN_BOX_SIZE
0   /* Hope the compilers complain about that */
#endif

kernel void
filter_inner (global float *input,
              global float *output)
{
    const int HALF_SIZE = (MEDIAN_BOX_SIZE - 1) / 2;

    /* Shift by half the median size to get correct index */
    int idx = get_global_id(0) + HALF_SIZE;
    int idy = get_global_id(1) + HALF_SIZE;

    /* Add median size to get correct width and height */
    int width = get_global_size(0) + MEDIAN_BOX_SIZE - 1;
    int height = get_global_size(1) + MEDIAN_BOX_SIZE - 1;

    float elements[MEDIAN_BOX_SIZE * MEDIAN_BOX_SIZE];

    /* Start at top left corner and pull out data */
    /* int index = (idy - HALF_SIZE) * width + idx - HALF_SIZE; */
    int i = 0;

    for (int y = idy - HALF_SIZE; y < idy + HALF_SIZE + 1; y++) {
        for (int x = idx - HALF_SIZE; x < idx + HALF_SIZE + 1; x++) {
            elements[i++] = input[y * width + x];
        }
    }

    /* Sort elements with (for now) insertion sort */
    for (int i = 1; i < MEDIAN_BOX_SIZE * MEDIAN_BOX_SIZE; i++) {
        int j = i;

        while (j > 0 && elements[j-1] > elements[j]) {
            float tmp = elements[j-1];
            elements[j-1] = elements[j];
            elements[j] = tmp;
            j--;
        }
    }

    output[idy * width + idx] = elements[MEDIAN_BOX_SIZE * MEDIAN_BOX_SIZE / 2];
}

kernel void
fill (global float *input,
      global float *output)
{
    const int HALF_SIZE = (MEDIAN_BOX_SIZE - 1) / 2;
    int idx = get_global_id(0);
    int idy = get_global_id(1);
    int width = get_global_size(0);
    int height = get_global_size(1);

    if (abs(idx - width) < HALF_SIZE && abs(idy - height) < HALF_SIZE)
        output[idy * width + idx] = input[idy * width + idx];
}
