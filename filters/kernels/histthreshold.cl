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


/* Naive implementation */
kernel void
histogram (global float *input,
           global float *output,
           unsigned int input_size,
           float min_range,
           float max_range)
{
    const int num_bins = get_global_size(0);
    const int bin = get_global_id(0);
    const float bin_width = (max_range - min_range) / num_bins;
    const float local_min = bin * bin_width;
    const float local_max = (bin + 1) * bin_width;
    int sum = 0;

    for (int i = 0; i < input_size; i++) {
        if ((local_min <= input[i]) && (input[i] < local_max))
            sum++;
    }

    output[bin] = ((float) sum) / input_size;
}

kernel void
threshold (global float *input,
           global float *histogram,
           global float *output)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int width = get_global_size(0);
    const int index = idy * width + idx;
    const float intensity = input[index];
    const int h_index = (int) (intensity * 256.0f);

    if (histogram[h_index] < 0.03f)
        output[index] = intensity;
    else
        output[index] = 0.0f;
}
