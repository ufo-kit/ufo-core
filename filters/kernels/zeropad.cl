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
 
typedef struct {
        float real;
        float imag;
} clFFT_Complex;

kernel void
zeropadding_kernel (global float *input, int offset, int in_width, global clFFT_Complex *output)
{
    const size_t out_width = get_global_size(0);
    const size_t idx = get_global_id(0);
    const size_t idy = get_global_id(1);
    const size_t index = idy * out_width + idx;

    int lidx = idx - (out_width - in_width / 2) - offset;

    output[index].imag = 0.0f;

    if (lidx >= 0) {
        output[index].real = input[idy * in_width + lidx];
    }
    else {
        lidx += out_width;
        output[index].real = lidx < in_width ? input[idy * in_width + lidx] : 0.0f;
    }
}

