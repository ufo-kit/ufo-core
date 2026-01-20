/*
 * Copyright (C) 2017 Karlsruhe Institute of Technology
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
cut (global float *input, global float *output, unsigned orig_width)
{
    const size_t x = get_global_id (0);
    const size_t y = get_global_id (1);
    const size_t width = get_global_size (0);
    const size_t idx = y * width + x;

    if (x < width / 2 + 1) {
        output[idx] = input[y * orig_width + x];
    }
    else {
        output[idx] = input[y * orig_width + orig_width + x - width];
    }
}
