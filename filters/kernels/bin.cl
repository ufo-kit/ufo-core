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

kernel void
binning_2d (global float *input,
            global float *output,
            const unsigned size,
            const unsigned in_width,
            const unsigned in_height)
{
    const size_t idx = get_global_id (0);
    const size_t idy = get_global_id (1);
    const size_t width = get_global_size (0);

    size_t index = (idy * size) * in_width + (idx * size);
    float sum;

    /* no loops for most common path, giving about 35% speed up */
    if (size == 2) {
        sum = input[index] + input[index + 1] +
              input[index + in_width] + input[index + in_width + 1];
    }
    else {
        sum = 0.0f;

        for (size_t j = 0; j < size; j++) {
            for (size_t i = 0; i < size; i++) {
                sum += input[index];
                index++;
            }

            /* next row */
            index += in_width - size;
        }
    }

    output[idy * width + idx] = sum;
}

kernel void
binning_3d (global float *input,
            global float *output,
            const unsigned size,
            const unsigned in_width,
            const unsigned in_height)
{
    const size_t idx = get_global_id (0);
    const size_t idy = get_global_id (1);
    const size_t idz = get_global_id (2);
    const size_t width = get_global_size (0);
    const size_t height = get_global_size (1);

    size_t index = (idz * size) * in_height * in_width + (idy * size) * in_width + (idx * size);
    float sum;

    if (size == 2) {
        sum = input[index] + input[index + 1];
        index = index + in_width;
        sum += input[index] + input[index + 1];
        index = index - in_width + in_width * in_height;
        sum += input[index] + input[index + 1];
        index = index + in_width;
        sum += input[index] + input[index + 1];
    }
    else {
        sum = 0.0f;

        for (size_t k = 0; k < size; k++) {
            for (size_t j = 0; j < size; j++) {
                for (size_t i = 0; i < size; i++) {
                    sum += input[index];
                    index++;
                }

                /* next row */
                index += in_width - size;
            }

            /* next plane */
            index += in_width * in_height - size * in_width;
        }
    }

    output[idz * height * width + idy * width + idx] = sum;
}
