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

kernel void
unsplit (global float *input,
         global float *output)
{
    size_t idx = get_global_id (0);
    size_t idy = get_global_id (1);
    size_t idz = get_global_id (2);
    size_t width = get_global_size (0);
    size_t height = get_global_size (1);
    size_t depth = get_global_size (2);

    output[idy * width * depth + idx * depth + idz] = input[idz * width * height + idy * width + idx];
}
