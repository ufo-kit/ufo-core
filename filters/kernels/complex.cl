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
c_add (global float *in1,
       global float *in2,
       global float *out)
{
    int idx = get_global_id(1) * 2 * get_global_size(0) + 2 * get_global_id(0);

    out[idx] = in1[idx] + in2[idx];
    out[idx+1] = in1[idx+1] + in2[idx+1];
}

kernel void
c_mul (global float *in1,
       global float *in2,
       global float *out)
{
    int idx = get_global_id(1) * 2 * get_global_size(0) + 2 * get_global_id(0);
    const float a = in1[idx];
    const float b = in1[idx+1];
    const float c = in2[idx];
    const float d = in2[idx+1];

    out[idx] = a*c - b*d;
    out[idx+1] = b*c + a*d;
}

kernel void
c_div (global float *in1,
       global float *in2,
       global float *out)
{
    int idx = get_global_id(1) * 2 * get_global_size(0) + 2 * get_global_id(0);
    const float a = in1[idx];
    const float b = in1[idx+1];
    const float c = in2[idx];
    const float d = in2[idx+1];
    float divisor = c*c + d*d;

    if (divisor == 0.0f)
        divisor = 0.000000001f;

    out[idx] = (a*c + b*d) / divisor;
    out[idx+1] = (b*c - a*d) / divisor;
}

kernel void
c_conj (global float *data,
        global float *out)
{
    int idx = get_global_id(1) * 2 * get_global_size(0) + 2 * get_global_id(0);
    out[idx] = data[idx];
    out[idx+1] = -data[idx+1];
}

/**
 * Compute power spectrum.
 *
 * @data: complex input
 * @out: real output
 */
kernel void
c_abs_squared (global float *data,
               global float *out)
{
    int width = get_global_size (0);
    int idx = get_global_id (0);
    int idy = get_global_id (1);
    int out_idx = width * idy + idx;
    /* Input data must be complex interleaved, global dimensions are with
     * respect to the real output, so multiply width by 2. */
    int in_idx = 2 * width * idy + 2 * idx;

    out[out_idx] = data[in_idx] * data[in_idx] + data[in_idx + 1] * data[in_idx + 1];
}

/**
  * c_mul_real_sym:
  * @frequencies: complex Fourier transform frequencies with interleaved
  * real/imaginary values
  * @output: multiplication result
  * @coefficients: first half of symmetric coefficients for the multiplication
  * (size = width / 2 + 1)
  *
  * Multiply every row of @frequencies with @coefficients which are half the *real*
  * width + 1, i.e. width = global size / 2 because of the complex numbers. This
  * kernel takes advantage of symmetry and expects @frequencies to be ordered as
  * [0, 1, ..., width / 2 - 1, -width / 2, ..., -1]. After width / 2 the @coefficients
  * are mirrored.
  */
kernel void
c_mul_real_sym (global float *frequencies,
                global float *output,
                constant float *coefficients)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int real_width = get_global_size (0) / 2;
    const int index = idy * 2 * real_width + idx;
    const int real_index = idx < real_width ? idx / 2 : real_width - idx / 2;

    output[index] = frequencies[index] * coefficients[real_index];
}
