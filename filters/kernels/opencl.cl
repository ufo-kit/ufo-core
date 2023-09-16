/*
 * Copyright (C) 2011-2017 Karlsruhe Institute of Technology
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

#ifndef SEARCH_RADIUS
#define SEARCH_RADIUS   10
#endif

#ifndef PATCH_RADIUS
#define PATCH_RADIUS    3
#endif

#ifndef SIGMA
#define SIGMA           12
#endif

#define flatten(x,y,r,w) ((y-r)*w + (x-r))

/* Compute the distance of two neighbourhood vectors _starting_ from index i
   and j and edge length radius */
float
dist (global float *input,
      int i,
      int j,
      int radius,
      int image_width)
{
    float dist = 0.0f, tmp;
    float wsize = (2.0f * radius + 1.0f);
    wsize *= wsize;

    const int nb_width = 2 * radius + 1;
    const int stride = image_width - nb_width;
    for (int k = 0; k < nb_width; k++, i += stride, j += stride) {
        for (int l = 0; l < nb_width; l++, i++, j++) {
            tmp = input[i] - input[j];
            dist += tmp * tmp;
        }
    }
    return dist / wsize;
}

kernel void
nlm_noise_reduction (global float *input,
                     global float *output)
{
    const int x = get_global_id (0);
    const int y = get_global_id (1);
    const int width = get_global_size (0);
    const int height = get_global_size (1);

    float total_weight = 0.0f;
    float pixel_value = 0.0f;

    /* 
     * Compute the upper left (sx,sy) and lower right (tx, ty) corner points of
     * our search window.
     */
    int r = min (PATCH_RADIUS, min(width - 1 - x, min (height - 1 - y, min (x, y))));
    int sx = max (x - SEARCH_RADIUS, r);
    int sy = max (y - SEARCH_RADIUS, r);
    int tx = min (x + SEARCH_RADIUS, width - 1 - r);
    int ty = min (y + SEARCH_RADIUS, height - 1 - r);

    for (int i = sx; i < tx; i++) {
        for (int j = sy; j < ty; j++) {
            float d = dist (input, flatten(x, y, r, width), flatten (i,j,r,width), r, width);
            float weight = exp (- (SIGMA * SIGMA) * d);
            pixel_value += weight * input[j * width + i];
            total_weight += weight;
        }
    }

    output[y * width + x] = pixel_value / total_weight;
}

kernel void
fix_nan_and_inf (global float *input,
                 global float *output)
{
    const size_t idx = get_global_id (1) * get_global_size (0) + get_global_id (0);
    float data = input[idx];

    if ((isnan (data) || isinf (data)))
        data = 0.0f;

    output[idx] = data;
}

kernel void
absorptivity (global float *input,
              global float *output)
{
    const size_t idx = get_global_id (1) * get_global_size (0) + get_global_id (0);
    output[idx] = -log (input[idx]);
}

kernel void
diff (global float *x,
      global float *y,
      global float *output)
{
    const size_t idx = get_global_id (1) * get_global_size (0) + get_global_id (0);
    output[idx] = x[idx] - y[idx];
}

kernel void
add (global float *x,
     global float *y,
     global float *output)
{
    const size_t idx = get_global_id (1) * get_global_size (0) + get_global_id (0);
    output[idx] = x[idx] + y[idx];
}

kernel void
multiply (global float *x,
          global float *y,
          global float *output)
{
    const size_t idx = get_global_id (1) * get_global_size (0) + get_global_id (0);
    output[idx] = x[idx] * y[idx];
}

kernel void
divide (global float *x,
        global float *y,
        global float *output)
{
    const size_t idx = get_global_id (1) * get_global_size (0) + get_global_id (0);
    output[idx] = x[idx] / y[idx];
}
