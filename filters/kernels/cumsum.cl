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
/* There are 16 memory banks, so divide n by their number, which offsets every
 * thread by n / 16 */
#define COMPUTE_SHIFTED_INDEX(n) ((n) + ((n) >> 4))

void sum_chunk_no_conflicts (global float *input,
                             global float *output,
                             local float *cache,
                             const int height_offset,
                             const int group_offset,
                             const int width,
                             const int local_width,
                             const int group_iterations,
                             const int gx,
                             const int lx)
{
    float tmp;
    int d, ai, bi, offset = 1;
    int tx_0 = height_offset + 2 * local_width * gx * group_iterations + 2 * lx + group_offset;

    if (tx_0 - height_offset < width) {
        cache[COMPUTE_SHIFTED_INDEX (2 * lx)] = input[tx_0];
    }
    if (tx_0 + 1 - height_offset < width) {
        cache[COMPUTE_SHIFTED_INDEX (2 * lx + 1)] = input[tx_0 + 1];
    }
    barrier (CLK_LOCAL_MEM_FENCE);

    // build sum in place up the tree
    for (d = local_width; d > 0; d >>= 1) {
        if (lx < d) {
            ai = COMPUTE_SHIFTED_INDEX (offset * (2 * lx + 1) - 1);
            bi = COMPUTE_SHIFTED_INDEX (offset * (2 * lx + 2) - 1);
            cache[bi] += cache[ai];
        }
        offset <<= 1;
        barrier (CLK_LOCAL_MEM_FENCE);
    }

    if (lx == local_width - 1) {
        tmp = group_offset == 0 ? 0 : output[height_offset + 2 * local_width * gx * group_iterations + group_offset - 1];
        cache[COMPUTE_SHIFTED_INDEX (2 * local_width)] = cache[COMPUTE_SHIFTED_INDEX (2 * local_width - 1)] + tmp;
        cache[COMPUTE_SHIFTED_INDEX (2 * local_width - 1)] = tmp;
    }

    // traverse down tree & build scan
    for (d = 1; d <= local_width; d <<= 1) {
        offset >>= 1;
        barrier (CLK_LOCAL_MEM_FENCE);
        if (lx < d) {
            ai = COMPUTE_SHIFTED_INDEX (offset * (2 * lx + 1) - 1);
            bi = COMPUTE_SHIFTED_INDEX (offset * (2 * lx + 2) - 1);
            tmp = cache[ai];
            cache[ai] = cache[bi];
            cache[bi] += tmp;
        }
    }
    barrier (CLK_LOCAL_MEM_FENCE);

    if (tx_0 - height_offset < width) {
        output[tx_0] = cache[bi];
    }
    if (tx_0 + 1 - height_offset < width) {
        output[tx_0 + 1] = cache[COMPUTE_SHIFTED_INDEX (2 * lx + 2)];
    }
}

kernel void cumsum (global float *input,
                    global float *output,
                    global float *block_sums,
                    local float *cache,
                    const int group_iterations,
                    const int width)
{
    int local_width = get_local_size (0);
    int gx = get_group_id (0);
    int lx = get_local_id (0);
    int idy = get_global_id (1);
    int height_offset = idy * width;

    for (int group_offset = 0; group_offset < 2 * local_width * group_iterations; group_offset += 2 * local_width) {
        sum_chunk_no_conflicts (input, output, cache, height_offset, group_offset, width, local_width, group_iterations, gx, lx);
        barrier (CLK_GLOBAL_MEM_FENCE);
    }
    if (block_sums && lx == 0) {
        block_sums[gx + 2 * local_width * idy] = output[height_offset +
                                                        min(width - 1, 2 * local_width * (gx + 1) * group_iterations - 1)];
    }
}

kernel void spread_block_sums (global float *output,
                               global float *block_sums,
                               const int group_iterations,
                               const int width)
{
    int local_width = get_local_size (0);
    int gx = get_group_id (0);
    int lx = get_local_id (0);
    int tx = local_width * ((gx + 1) * group_iterations) + lx;
    int idy = get_global_id (1);

    for (int group_offset = 0; group_offset < local_width * group_iterations; group_offset += local_width) {
        if (tx + group_offset >= width) {
            break;
        }
        output[idy * width + tx + group_offset] += block_sums[gx + local_width * idy];
    }
}
