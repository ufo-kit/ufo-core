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
cut_sinogram (global float *input, int offset, global float *output)
{
	const uint global_x_size = get_global_size(0);

	int g_idx = get_global_id(0);
	int g_idy = get_global_id(1);

	output[g_idy * global_x_size + g_idx] = input[g_idy * (global_x_size + offset) + g_idx + offset];
}
