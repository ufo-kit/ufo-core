/*
 * Copyright (C) 2015-2016 Karlsruhe Institute of Technology
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
flip_horizontal (global float *input, global float *output)
{
    size_t idx = get_global_id (0);
    size_t idy = get_global_id (1);
    size_t width = get_global_size (0);
    size_t height = get_global_size (1);

    output[idy * width + (width - idx - 1)] = input[idy * width + idx];
}

kernel void
flip_vertical (global float *input, global float *output)
{
    size_t idx = get_global_id (0);
    size_t idy = get_global_id (1);
    size_t width = get_global_size (0);
    size_t height = get_global_size (1);

    output[(height - idy - 1) * width + idx] = input[idy * width + idx];
}
