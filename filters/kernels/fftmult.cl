/*
 * Copyright (C) 2013-2016 Karlsruhe Institute of Technology
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

kernel
void mult(global float* a, global float* b, global float* res)
{
    int width = get_global_size(0) * 2;
    int x = get_global_id(0) * 2;
    int y = get_global_id(1);
    int idx_r = x + y * width;
    int idx_i = 1 + x + y * width;

    float ra = a[idx_r];
    float rb = b[idx_r];
    float ia = a[idx_i];
    float ib = b[idx_i];

    res[idx_r] = ra * rb - ia * ib;
    res[idx_i] = ra * ib + rb * ia;
}
