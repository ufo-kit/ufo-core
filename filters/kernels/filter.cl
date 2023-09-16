/*
 * Copyright (C) 2011-2013 Karlsruhe Institute of Technology
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
filter (global float *input,
        global float *output,
        global float *filter)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int index = idy*get_global_size(0) + idx;
    output[index] = input[index] * filter[idx];
}

kernel void
stripe_filter (global float *input,
               global float *output,
               const float horizontal_sigma,
               const float vertical_sigma)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int width = get_global_size(0);
    const int height = get_global_size(1);
    const int index = idy * get_global_size(0) + idx;
    /* Swap frequencies for interleaved complex input */
    /* No need to negate the second half of frequencies for Gaussian */
    const float x = idx >= width >> 1 ? (width >> 1) - (idx >> 1) : idx >> 1;
    const float x_weight = exp (- x * x / (2 * horizontal_sigma * horizontal_sigma));

    if (vertical_sigma == 0.0f) {
        if (idy == 0) {
            output[index] = input[index] * x_weight;
        } else {
            output[index] = input[index];
        }
    } else {
        const float y = idy >= height >> 1 ? height - idy : idy;
        const float y_weight = exp (- y * y / (2 * vertical_sigma * vertical_sigma));
        output[index] = input[index] * (1 - y_weight * (1 - x_weight));
    }
}
