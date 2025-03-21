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
fft_spread (global float *out,
            global float *in,
            const int width,
            const int height)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int idz = get_global_id(2);
    const int len_x = get_global_size(0);
    const int len_y = get_global_size(1);

    const int stride_x = len_x * 2;
    const int stride_y = stride_x * len_y;
    const int stride_x_in = width;
    const int stride_y_in = stride_x_in * height;

    /* May diverge but not possible to reduce latency, because num_bins can
       be arbitrary and not be aligned. */
    if ((idy >= height) || (idx >= width)) {
        out[idz*stride_y + idy*stride_x + idx*2] = 0.0f;
        out[idz*stride_y + idy*stride_x + idx*2 + 1] = 0.0f;
    }
    else {
        out[idz*stride_y + idy*stride_x + idx*2] = in[idz*stride_y_in + idy*stride_x_in + idx];
        out[idz*stride_y + idy*stride_x + idx*2 + 1] = 0.0f;
    }
}

kernel void
fft_pack (global float *in,
          global float *out,
          const int width,
          const int height,
          const float scale)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int idz = get_global_id(2);

    const int len_x = get_global_size(0);
    const int len_y = get_global_size(1);

    const int stride_x_in = len_x * 2;
    const int stride_y_in = len_y * stride_x_in;

    const int stride_x = width;
    const int stride_y = height * stride_x;

    if (idx  < width && idy < height)
        out[idz*stride_y + idy*stride_x + idx]
            = in[idz*stride_y_in + idy*stride_x_in + idx*2] * scale;
}

kernel void
fft_normalize (global float *data)
{
    const int idx = get_global_id(0);
    const int dim_fft = get_global_size(0);
    data[2*idx] = data[2*idx] / dim_fft;
}
