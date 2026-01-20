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

kernel void
rotate_image (image2d_t input,
              global float *output,
              const sampler_t sampler,
              const float2 sincos,
              const float2 center,
              const float2 padded_center,
              const int2 input_shape)
{
    int idx = get_global_id (0);
    int idy = get_global_id (1);
    float x = idx - padded_center.x + 0.5f;
    float y = idy - padded_center.y + 0.5f;

    output[idy * get_global_size (0) + idx] =
        read_imagef (input, sampler,
                     (float2) ((sincos.y * x - sincos.x * y + center.x) / input_shape.x,
                               (sincos.x * x + sincos.y * y + center.y) / input_shape.y)).x;
}
