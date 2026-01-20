/*
 * Copyright (C) 2011-2014 Karlsruhe Institute of Technology
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
 * populate_polar_space:
 * @input: input image in cartesian coordinates
 * @output: output buffer in polar coordinates
 * @sampler: a sampler object
 *
 * Populate polar space by converting the coordinates to cartesian and fetching data from
 * the input. Global indices in this kernel represent polar space
 * (x - distance, y - angular index).
 */
kernel void
populate_polar_space (read_only image2d_t input,
                      global float *output,
                      sampler_t sampler)
{
    int2 id = (int2) (get_global_id (0), get_global_id (1));
    float angle = (2.0f * id.y / get_global_size (1) - 1.0f) * M_PI_F;
    float2 pixel = (float2) (id.x * cos (angle) + get_image_width (input) / 2.0f,
                             id.x * sin (angle) + get_image_height (input) / 2.0f);

    output[id.y * get_global_size (0) + id.x] = read_imagef (input, sampler, pixel).x;
}

/**
 * populate_cartesian_space:
 * @input: input image in polar coordinates
 * @output: output buffer in cartesian coordinates
 * @sampler: a sampler object
 *
 * Populate cartesian space by converting the coordinates to polar and fetching data from
 * the input. Global indices in this kernel represent cartesian space.
 */
kernel void
populate_cartesian_space (read_only image2d_t input,
                          global float *output,
                          sampler_t sampler)
{
    int2 id = (int2) (get_global_id (0), get_global_id (1));
    int2 shape = (int2) (get_global_size (0), get_global_size (1));
    float2 pixel = (float2) (id.x - shape.x / 2, id.y - shape.y / 2);
    float angle = atan2 (pixel.y, pixel.x) + M_PI_F;
    float2 polar_pixel = (float2) (sqrt (pixel.x * pixel.x + pixel.y * pixel.y),
                                   angle * get_image_height (input) / (2 * M_PI_F));

    output[id.y * shape.x + id.x] = read_imagef (input, sampler, polar_pixel).x;
}
