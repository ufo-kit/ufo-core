/*
 * Copyright (C) 2014-2017 Karlsruhe Institute of Technology
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

#ifndef BOX_SIZE
#define BOX_SIZE 3
#endif

kernel void
filter (global float *input,
              global float *output,
              const float o_threshold,
              const int o_sign)
{
    const int HALF_SIZE = (BOX_SIZE - 1) / 2;
    const int ARR_SIZE = BOX_SIZE * BOX_SIZE;

    int idx = get_global_id(0);
    int idy = get_global_id(1);

    int width = get_global_size(0);
    int height = get_global_size(1);

    /* Check for boundary elements */
    if (idy < HALF_SIZE || idy + HALF_SIZE >= height || idx < HALF_SIZE || idx + HALF_SIZE >= width)
        output[idy * width + idx] = input[idy * width + idx];
    else {
        float elements[BOX_SIZE * BOX_SIZE];

        /* Start at top left corner and pull out data */
        /* int index = (idy - HALF_SIZE) * width + idx - HALF_SIZE; */
        int i = 0;

        for (int y = idy - HALF_SIZE; y < idy + HALF_SIZE + 1; y++) {
            for (int x = idx - HALF_SIZE; x < idx + HALF_SIZE + 1; x++) {
                elements[i++] = input[y * width + x];
            }
        }
        /* Sort elements with (for now) insertion sort */
        for (int i = 1; i < ARR_SIZE; i++) {
            int j = i;
            while (j > 0 && elements[j-1] > elements[j]) {
                float tmp = elements[j-1];
                elements[j-1] = elements[j];
                elements[j] = tmp;
                j--;
            }
        }
        if (input[idy * width + idx]*o_sign > elements[(ARR_SIZE - 1)/ 2]*o_sign + o_threshold) {
            float median_unbiased = ( elements[(ARR_SIZE - 1) / 2] + elements[(ARR_SIZE - 1) / 2 - o_sign] ) / 2 ;
            output[idy * width + idx] = (float)(median_unbiased);
        }
        else {
            output[idy * width + idx] = input[idy * width + idx];
        }
    }
}
