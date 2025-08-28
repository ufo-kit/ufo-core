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
#define PIXELS_PER_THREAD 4

/* Coalesced transposition thanks to transposing a) tile indices and b) data in
 * shared memory. The algorithm requires square work group size in order not to
 * need to transpose thread x and y indices. Every thread processes
 * PIXELS_PER_THREAD items to better utilize the GPU. Thus, the work group size
 * is square with respect to the shared memory, but with respect to the
 * execution model its dimensions are (group_width, group_width /
 * PIXELS_PER_THREAD). *width* and *height* are real input size which needs to
 * be respected (the global work size in both dimensions is set in a way to
 * respect the work group size).
 */
kernel void transpose_shared (global float *input,
                              global float *output,
                              local float *cache,
                              const int width,
                              const int height)
{
    int idx = get_global_id (0);
    int lx = get_local_id (0);
    /* Every work item processes indices [ly, ly + 3] */
    int ly = get_local_id (1) * PIXELS_PER_THREAD;
    /* Also height */
    int group_width = get_local_size (0);
    int idy = get_group_id (1) * group_width + ly;
    /* Precompute transposed index to the shared memory */
    /* (group_width + 1) to prevent shared memory bank conflicts */
    int j, max_j, cache_offset = lx * (group_width + 1) + ly;

    /* Do not terminate the complete kernel here because the threads which lead
     * to *idx* might be needed in the output part of the kernel. */
    if (idx < width) {
        /* Don't step outside the image */
        max_j = min (PIXELS_PER_THREAD, height - idy);
        #pragma unroll PIXELS_PER_THREAD
        for (j = 0; j < max_j; j++) {
            cache[(ly + j) * (group_width + 1) + lx] = input[(idy + j) * width + idx];
        }
    }

    barrier (CLK_LOCAL_MEM_FENCE);

    /* Transpose tile index but process it in a row-based fashion to keep writes coalesced */
    idx = get_group_id (1) * group_width + lx;
    if (idx >= height) {
        return;
    }
    idy = get_group_id (0) * group_width + ly;
    max_j = min (PIXELS_PER_THREAD, width - idy);

    #pragma unroll PIXELS_PER_THREAD
    for (j = 0; j < max_j; j++) {
        output[(idy + j) * height + idx] = cache[cache_offset + j];
    }
}
