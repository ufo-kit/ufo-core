/*
 * Copyright (C) 2011-2015 Karlsruhe Institute of Technology
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

kernel void pad (read_only image2d_t in_image,
                 sampler_t sampler,
                 global float *result,
                 const int2 input_shape,
                 const int2 offset)
{
    int2 pixel = (int2) (get_global_id (0), get_global_id (1));
    float2 norm_pixel = (float2) (((float) pixel.x - offset.x) / input_shape.x,
                                  ((float) pixel.y - offset.y) / input_shape.y);

    result[pixel.y * get_global_size(0) + pixel.x] = read_imagef(in_image, sampler, norm_pixel).x;
}


/*
 * Pad *output* with *input*. *weight* is used by stitching to normalize one
 * image's mean to the other.
 */
kernel void pad_with_image (global float *input,
                            global float *output,
                            const int input_offset,
                            const int output_offset,
                            const int input_width,
                            const int output_width,
                            const float weight)
{
    int idx = get_global_id (0);
    int idy = get_global_id (1);

    output[idy * output_width + idx + output_offset] = weight * input[idy * input_width + idx + input_offset];
}
