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

constant sampler_t image_sampler_ktbl = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_LINEAR;
constant sampler_t image_sampler_data = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;

typedef struct {
    float real;
    float imag;
} clFFT_Complex;

kernel void
clear_kernel(global clFFT_Complex *output)
{
    int global_x_size = get_global_size(0);
    int local_x_id = get_global_id(0);
    int local_y_id = get_global_id(1);

    output[local_y_id * global_x_size + local_x_id].real = 0.0f;
    output[local_y_id * global_x_size + local_x_id].imag = 0.0f;
}

kernel void
dfi_sinc_kernel(read_only image2d_t input,
                read_only image2d_t ktbl,
                float L2,
                int ktbl_len2,
                int raster_size,
                float table_spacing,
                float angle_step_rad,
                float theta_max,
                float rho_max,
                float max_radius,
                int spectrum_offset,
                global clFFT_Complex *output)
{
    float2 out_coord, norm_gl_coord, in_coord, ktbl_coord, id_data_coord;
    int iul, iuh, ivl, ivh, sgn, i, j, k;
    float res_real, res_imag, weight, kernel_x_val;
    long out_idx;
    const int raster_size_half = raster_size / 2;

    ktbl_coord.y = 0.5f;
    sgn = 1;
    res_real = 0.0f, res_imag = 0.0f;
    out_idx = 0;

    out_coord.x = get_global_id(0) + spectrum_offset;
    out_coord.y = get_global_id(1) + spectrum_offset;

    if (out_coord.y > raster_size_half)
        return;

    out_idx = out_coord.y * raster_size + out_coord.x;

    norm_gl_coord.x = out_coord.x - raster_size_half;
    norm_gl_coord.y = out_coord.y - raster_size_half;

    // calculate coordinates
    const float radius = sqrt(norm_gl_coord.x * norm_gl_coord.x + norm_gl_coord.y * norm_gl_coord.y);

    if (radius > max_radius)
        return;

	long x_offset = (raster_size_half) - out_coord.x;
	long y_offset = (raster_size_half) - out_coord.y;

	long out_idx_mirror = 0;

	out_idx_mirror = (raster_size_half + y_offset) * raster_size + (raster_size_half + x_offset);

	if (out_idx_mirror < 0 || out_idx_mirror > (raster_size * raster_size) - 1)
	    return;

    in_coord.y = atan2(norm_gl_coord.y,norm_gl_coord.x);
    in_coord.y = -in_coord.y; // spike here! (mirroring along y-axis)

    sgn = (in_coord.y < 0.0f) ? -1 : 1;

    in_coord.y = (in_coord.y < 0.0f) ? in_coord.y += M_PI_F : in_coord.y;
    in_coord.y = (float) min(1.0f + in_coord.y / angle_step_rad, theta_max - 1);
    in_coord.x = (float) min(radius, (float) raster_size_half);

    //sinc interpolation
    iul = (int) ceil(in_coord.x - L2);
    iul = (iul < 0) ? 0 : iul;

    iuh = (int) floor(in_coord.x + L2);
    iuh = (iuh > rho_max - 1) ? iuh = rho_max - 1 : iuh;

    ivl = (int) ceil(in_coord.y - L2);
    ivl = (ivl < 0) ? 0 : ivl;

    ivh = (int) floor(in_coord.y + L2);
    ivh = (ivh > theta_max - 1) ? ivh = theta_max - 1 : ivh;

    float kernel_y[20];

    for (i = ivl, j = 0; i <= ivh; ++i, ++j) {
        ktbl_coord.x = ktbl_len2 + (in_coord.y - (float)i) * table_spacing;
        kernel_y[j] = read_imagef(ktbl, image_sampler_ktbl, ktbl_coord).s0;
    }

    for (i = iul; i <= iuh; ++i) {
        ktbl_coord.x = ktbl_len2 + (in_coord.x - (float)i) * table_spacing;
        kernel_x_val = read_imagef(ktbl, image_sampler_ktbl, ktbl_coord).s0;

        for (k = ivl, j = 0; k <= ivh; ++k, ++j) {
            weight = kernel_y[j] * kernel_x_val;

            id_data_coord.x = i;
            id_data_coord.y = k;

            float4 cpxl = read_imagef(input, image_sampler_data, id_data_coord);

            res_real += cpxl.x * weight;
            res_imag += cpxl.y * weight;
        }
    }

    output[out_idx].real = res_real;
    output[out_idx].imag = sgn * res_imag;

    /* Point reflection mirroring / Hermitian symmetry */
    if (out_coord.y != 0 || out_coord.x > raster_size) {
        if (out_coord.x == 0 && 0 < out_coord.y <= raster_size_half) {
            out_idx = ((raster_size_half + 2) + y_offset) * raster_size;

            output[out_idx].real = res_real;
            output[out_idx].imag = -sgn * res_imag;
        }
        else {
            if (res_imag == 0.0f) {
                output[out_idx_mirror].real = res_real;
                output[out_idx_mirror].imag = sgn * res_imag;
            }
            else {
                output[out_idx_mirror].real = res_real;
                output[out_idx_mirror].imag = -sgn * res_imag;
            }
        }
    }
}
