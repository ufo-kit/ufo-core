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
swap_quadrants_kernel_real(global float *input, global float *output)
{
	const uint global_x_size = get_global_size(0);

	int global_x_size2 = global_x_size/2;
	int global_y_size2 = global_x_size2;

	int g_idx = get_global_id(0);
	int g_idy = get_global_id(1);
			
	if (g_idx < global_x_size2) {
		output[g_idy * global_x_size + g_idx] = input[(global_y_size2 + g_idy) * global_x_size + (global_x_size2 + g_idx)];
		output[(global_y_size2 + g_idy) * global_x_size + (global_x_size2 + g_idx)] = input[g_idy * global_x_size + g_idx];
	}
	else {
		output[g_idy * global_x_size + g_idx] = input[(global_y_size2 + g_idy) * global_x_size + (g_idx - global_x_size2)];
		output[(global_y_size2 + g_idy) * global_x_size + (g_idx - global_x_size2)] = input[g_idy * global_x_size + g_idx];
	}
}

kernel void
swap_quadrants_kernel_complex(global clFFT_Complex *input, global clFFT_Complex *output)
{
	const uint global_x_size = get_global_size(0);

	int global_x_size2 = global_x_size/2;
	int global_y_size2 = global_x_size2;

	int g_idx = get_global_id(0);
	int g_idy = get_global_id(1);
			
	if (g_idx < global_x_size2) {
		output[g_idy * global_x_size + g_idx] = input[(global_y_size2 + g_idy) * global_x_size + (global_x_size2 + g_idx)];
		output[(global_y_size2 + g_idy) * global_x_size + (global_x_size2 + g_idx)] = input[g_idy * global_x_size + g_idx];
	}
	else {
		output[g_idy * global_x_size + g_idx] = input[(global_y_size2 + g_idy) * global_x_size + (g_idx - global_x_size2)];
		output[(global_y_size2 + g_idy) * global_x_size + (g_idx - global_x_size2)] = input[g_idy * global_x_size + g_idx];
	}
}
