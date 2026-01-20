/*
 * Copyright (C) 2011-2013 Karlsruhe Institute of Technology
 *
 * The algorithmic part is designed by Julian Moosmann, Institute
 * for Photon Science and Synchrotron Radiation.
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

#define COMMON_FREQUENCY_SETUP                          \
    const int width = get_global_size(0);               \
    const int height = get_global_size(1);              \
    int idx = get_global_id(0);                         \
    int idy = get_global_id(1);                         \
    float n_idx = (idx >= width >> 1) ? idx - width : idx; \
    float n_idy = (idy >= height >> 1) ? idy - height : idy; \
    n_idx = n_idx / width; \
    n_idy = n_idy / height;

#define COMMON_SETUP_TIE                                                     \
    COMMON_FREQUENCY_SETUP;                                                  \
    float sin_arg = prefac.x * (n_idx * n_idx) + prefac.y * (n_idy * n_idy); \

#define COMMON_SETUP    \
    COMMON_SETUP_TIE;   \
    float sin_value = sin(sin_arg);

#define CTF_MULTI_SETUP                                                                                                     \
    float prefac = M_PI * wavelength * (n_idx * n_idx / pixel_size / pixel_size + n_idy * n_idy / pixel_size / pixel_size);

/* b/d * cos + sin = d/b * (b/d * sin + cos); 10^R = d/b */
#define CTF_MULTI_VALUE(prefac, dist, regularize_rate) (pow(10, (regularize_rate)) * sin ((prefac) * (dist)) + cos ((prefac) * (dist)))


kernel void
tie_method(float2 prefac, float regularize_rate, float binary_filter_rate, float frequency_cutoff, global float *output)
{
    COMMON_SETUP_TIE;
    if (sin_arg >= frequency_cutoff)
        output[idy * width + idx] = 0.0f;
    else
        output[idy * width + idx] = 0.5f / (sin_arg + pow(10, -regularize_rate));
}

kernel void
ctf_method(float2 prefac, float regularize_rate, float binary_filter_rate, float frequency_cutoff, global float *output)
{
    COMMON_SETUP;

    if (fabs (sin_value + pow(10, -regularize_rate)) < 1e-7 || sin_arg >= frequency_cutoff || (idx == 0 && idy == 0))
        output[idy * width + idx] = 0.0f;
    else
        output[idy * width + idx] = 0.5f / (sin_value + pow(10, -regularize_rate));
}

kernel void
qp_method(float2 prefac, float regularize_rate, float binary_filter_rate, float frequency_cutoff, global float *output)
{
    COMMON_SETUP;

    if (sin_arg > M_PI_2_F && fabs (sin_value + pow(10, -regularize_rate)) < binary_filter_rate ||
        sin_arg >= frequency_cutoff || (idx == 0 && idy == 0))
        /* Zero frequency (idx == 0 && idy == 0) must be set to zero explicitly */
        output[idy * width + idx] = 0.0f;
    else
        output[idy * width + idx] = 0.5f / (sin_value + pow(10, -regularize_rate));
}

kernel void
qp2_method(float2 prefac, float regularize_rate, float binary_filter_rate, float frequency_cutoff, global float *output)
{
    COMMON_SETUP;
    float cacl_filter_value = 0.5f / (sin_value + pow(10, -regularize_rate));

    if (sin_arg > M_PI_2_F && fabs(sin_value + pow(10, -regularize_rate)) < binary_filter_rate ||
        sin_arg >= frequency_cutoff || (idx == 0 && idy == 0))
        output[idy * width + idx] = sign(cacl_filter_value) / (2 * (binary_filter_rate + pow(10, -regularize_rate)));
    else
        output[idy * width + idx] = cacl_filter_value;
}

kernel void
mult_by_value(global float *input, global float *values, global float *output)
{
    int idx = get_global_id(1) * get_global_size(0) + get_global_id(0);
    /* values[idx >> 1] because the filter is real (its width is *input* width / 2)
     * and *input* is complex with real (idx) and imaginary part (idx + 1)
     * interleaved. Thus, two consecutive *input* values are multiplied by the
     * same filter value. */
    output[idx] = input[idx] * values[idx >> 1];
}

/**
 * Compute the sum of the CTF^2 for the later division.
 */
kernel void
ctf_multidistance_square(global float *distances,
                         unsigned int num_distances,
                         float wavelength,
                         float pixel_size,
                         float regularize_rate,
                         global float *output)
{
    COMMON_FREQUENCY_SETUP;
    CTF_MULTI_SETUP;
    float result = 0.0f;
    float value;

    for (unsigned int i = 0; i < num_distances; i++) {
        value = CTF_MULTI_VALUE(prefac, distances[i], regularize_rate);
        result += value * value;
    }

    output[idy * width + idx] = num_distances * 0.5f * pow(10, regularize_rate) / (result + pow(10, -regularize_rate));
}

/**
 * Apply the CTF to a projection at a specific distance.
 */
kernel void
ctf_multidistance_apply_distance(global float *input,
                                 float dist,
                                 unsigned int num_distances,
                                 float wavelength,
                                 float pixel_size,
                                 float regularize_rate,
                                 global float *output)
{
    COMMON_FREQUENCY_SETUP;
    CTF_MULTI_SETUP;
    int index = idy * 2 * width + 2 * idx;
    float value = CTF_MULTI_VALUE(prefac, dist, regularize_rate) / num_distances;

    output[index] += input[index] * value;
    output[index + 1] += input[index + 1] * value;
}
