/*
 * Copyright (C) 2011-2019 Karlsruhe Institute of Technology
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
convolve_abs_laplacian_diff (read_only image2d_t input,
                             const sampler_t sampler,
                             global float *output)
{
    int idx = get_global_id (0);
    int idy = get_global_id (1);
    int width = get_global_size (0);
    int height = get_global_size (1);
    float sum = 0.0f;
    float coeffs[3][3] = {{ 1.0f, -2.0f,  1.0f},
                        {-2.0f,  4.0f, -2.0f},
                        { 1.0f, -2.0f,  1.0f}};

    for (int dy = -1; dy < 2; dy++) {
        for (int dx = -1; dx < 2; dx++) {
            sum += coeffs[dy + 1][dx + 1] * read_imagef (input, sampler, (float2) ((idx + dx + 0.5f) / width,
                                                                                   (idy + dy + 0.5f) / height)).x;
        }
    }

    output[idy * width + idx] = fabs (sum);
}
