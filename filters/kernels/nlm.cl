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

float
compute_dist (read_only image2d_t input,
              sampler_t sampler,
              float2 p,
              float2 q,
              int radius,
              int width,
              int height,
              float h_2,
              float variance,
              constant float *window_coeffs)
{
    float dist = 0.0f, tmp;
    int wsize = (2 * radius + 1);
    float coeff = h_2;

    if (!window_coeffs) {
        /* Gaussian window is normalized to sum=1, so if it is used, we are done
         * with just summation. If it is not, we need to compute the mean. */
        coeff /= wsize * wsize;
    }

    for (int j = -radius; j < radius + 1; j++) {
        for (int i = -radius; i < radius + 1; i++) {
            tmp = read_imagef (input, sampler, (float2) ((p.x + i) / width, (p.y + j) / height)).x -
                    read_imagef (input, sampler, (float2) ((q.x + i) / width, (q.y + j) / height)).x;
            if (window_coeffs) {
                dist += window_coeffs[(j + radius) * wsize + (i + radius)] * (tmp * tmp - 2 * variance);
            } else {
                dist += tmp * tmp - 2 * variance;
            }
        }
    }

    return fmax (0.0f, dist) * coeff;
}

kernel void
compute_shifted_mse (read_only image2d_t input,
                     global float *output,
                     sampler_t sampler,
                     const int dx,
                     const int dy,
                     const int padding,
                     const float variance)
{
    /* Global dimensions are for the padded output image */
    int idx = get_global_id (0);
    int idy = get_global_id (1);
    int padded_width = get_global_size (0);
    /* Image is padded by *padding* on both sides of both dimensions. */
    float cropped_width = (float) (padded_width - 2 * padding);
    float cropped_height = (float) (get_global_size (1) - 2 * padding);
    float x = ((float) (idx - padding)) + 0.5f;
    float y = ((float) (idy - padding)) + 0.5f;
    float a, b;

    a = read_imagef (input, sampler, (float2) (x / cropped_width, y / cropped_height)).x;
    b = read_imagef (input, sampler, (float2) ((x + dx) / cropped_width, (y + dy) / cropped_height)).x;

    output[idy * padded_width + idx] = (a - b) * (a - b) - 2 * variance;
}

kernel void
process_shift (read_only image2d_t input,
               global float *integral_input,
               global float *weights,
               global float *output,
               const sampler_t sampler,
               const int dx,
               const int dy,
               const int patch_radius,
               const int padding,
               const float coeff,
               const int is_conjugate)
{
    /* Global dimensions are for the cropped output image */
    int idx = get_global_id (0);
    int idy = get_global_id (1);
    /* Image is padded by *padding* on both sides of both dimensions. */
    int x = idx + padding;
    int y = idy + padding;
    float x_im = idx + dx + 0.5f;
    float y_im = idy + dy + 0.5f;
    int cropped_width = get_global_size (0);
    int cropped_height = get_global_size (1);
    int padded_width = cropped_width + 2 * padding;
    float dist, weight;
    if (is_conjugate) {
        /* direct computation: g(x) = w(x) + f(x + dx)
         * conjugate: g(x + dx) = w(x) + f(x), where idx = x + dx, so
         * g(idx) = w(idx - dx) * f(idx - x) = w(idx - dx) * f(x_im - 2dx)
         */
        x -= dx;
        y -= dy;
        x_im -= 2 * dx;
        y_im -= 2 * dy;
    }

    dist = integral_input[(y + patch_radius) * padded_width + x + patch_radius] -
           integral_input[(y - patch_radius - 1) * padded_width + x + patch_radius] -
           integral_input[(y + patch_radius) * padded_width + x - patch_radius - 1] +
           integral_input[(y - patch_radius - 1) * padded_width + x - patch_radius - 1];
    dist = fmax (0.0f, dist) * coeff;
    weight = exp (-dist);

    weights[idy * cropped_width + idx] += weight;
    output[idy * cropped_width + idx] += weight * read_imagef (input, sampler, (float2) (x_im / cropped_width,
                                                                                         y_im / cropped_height)).x;
}

kernel void divide_inplace (global float *coefficients,
                            global float *output)
{
    int index = get_global_size (0) * get_global_id (1) + get_global_id (0);

    output[index] = native_divide (output[index], coefficients[index]);
}

kernel void
nlm_noise_reduction (read_only image2d_t input,
                     global float *output,
                     sampler_t sampler,
                     const int search_radius,
                     const int patch_radius,
                     const float h_2,
                     const float variance,
                     constant float *window_coeffs)
{
    const int x = get_global_id (0);
    const int y = get_global_id (1);
    const int width = get_global_size (0);
    const int height = get_global_size (1);
    float d, weight;

    float total_weight = 0.0f;
    float pixel_value = 0.0f;

    for (int j = y - search_radius; j < y + search_radius + 1; j++) {
        for (int i = x - search_radius; i < x + search_radius + 1; i++) {
            d = compute_dist (input, sampler, (float2) (x + 0.5f, y + 0.5f), (float2) (i + 0.5f, j + 0.5f),
                      patch_radius, width, height, h_2, variance, window_coeffs);
            weight = exp (-d);
            pixel_value += weight * read_imagef (input, sampler, (float2) ((i + 0.5f) / width, (j + 0.5f) / height)).x;
            total_weight += weight;
        }
    }

    output[y * width + x] = pixel_value / total_weight;
}
