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

#define M_SUM(a, b, c) ((a) + (b))
#define M_MIN(a, b, c) (((a) < (b)) ? (a) : (b))
#define M_MAX(a, b, c) (((a) > (b)) ? (a) : (b))
#define M_SQUARE(a, b, c) ((a) + (((b) - (c)) * ((b) - (c))))
#define M_CUBE(a, b, c) ((a) + (((b) - (c)) * ((b) - (c)) * ((b) - (c))))
#define M_QUADRATE(a, b, c) ((a) + (((b) - (c)) * ((b) - (c)) * ((b) - (c)) * ((b) - (c))))

#define DEFINE_PARALLEL_KERNEL(red_operation, sum_operation, default_value)                  \
kernel void reduce_##red_operation (global float *input,                                     \
                                    global float *output,                                    \
                                    global float *param,                                     \
                                    local float *cache,                                      \
                                    const ulong real_size,                                   \
                                    const int pixels_per_thread)                             \
{                                                                                            \
    int lid = get_local_id (0);                                                              \
    size_t gid = get_global_id (0);                                                          \
    size_t global_size = get_global_size (0);                                                \
    float value = default_value;                                                             \
                                                                                             \
    for (int i = 0; i < pixels_per_thread; i++) {                                            \
        if (gid < real_size) {                                                               \
            value = red_operation(value, input[gid], param[0]);                              \
        }                                                                                    \
        gid += global_size;                                                                  \
    }                                                                                        \
                                                                                             \
    cache[lid] = value;                                                                      \
    barrier (CLK_LOCAL_MEM_FENCE);                                                           \
                                                                                             \
    for (int block = get_local_size (0) >> 1; block > 0; block >>= 1) {                      \
        if (lid < block) {                                                                   \
            cache[lid] = sum_operation(cache[lid], cache[lid + block], 0);                   \
        }                                                                                    \
        barrier (CLK_LOCAL_MEM_FENCE);                                                       \
    }                                                                                        \
                                                                                             \
    if (lid == 0) {                                                                          \
        output[get_group_id (0)] = cache[0];                                                 \
    }                                                                                        \
}

// There is at least 5 % performance drop in case we use ulong for storing dimension sizes
#define DEFINE_PARALLEL_KERNEL_0(red_operation, sum_operation, default_value)                           \
kernel void reduce_0##red_operation (global float *input,                                               \
                                     global float *output,                                              \
                                     global float *param,                                               \
                                     local float *cache,                                                \
                                     const int width,                                                   \
                                     const int height,                                                  \
                                     const int input_width,                                             \
                                     const int output_width,                                            \
                                     const int pixels_per_thread)                                       \
{                                                                                                       \
    int lx = get_local_id (0);                                                                          \
    int ly = get_local_id (1);                                                                          \
    int group_width = get_local_size (0);                                                               \
    int group_height = get_local_size (1);                                                              \
    int idx = get_group_id (0) * group_width * pixels_per_thread + lx;                                  \
    int idy = get_global_id (1);                                                                        \
    int max_i = min (pixels_per_thread, (width - idx - 1) / group_width + 1);                           \
    float value = default_value;                                                                        \
                                                                                                        \
    if (idy >= height || idx >= width) {                                                                \
        cache[ly * group_width + lx] = default_value;                                                   \
        return;                                                                                         \
    }                                                                                                   \
                                                                                                        \
    for (int i = 0; i < max_i; i++) {                                                                   \
        value = red_operation (value,                                                                   \
                               input[idy * input_width + idx + i * group_width],                        \
                               param[idy]);                                                             \
    }                                                                                                   \
                                                                                                        \
    cache[ly * group_width + lx] = value;                                                               \
    barrier (CLK_LOCAL_MEM_FENCE);                                                                      \
                                                                                                        \
    for (int block = group_width >> 1; block > 0; block >>= 1) {                                        \
        if (lx < block) {                                                                               \
            cache[ly * group_width + lx] = sum_operation (cache[ly * group_width + lx],                 \
                                                          cache[ly * group_width + lx + block],         \
                                                          0);                                           \
        }                                                                                               \
        barrier (CLK_LOCAL_MEM_FENCE);                                                                  \
    }                                                                                                   \
                                                                                                        \
    if (lx == 0) {                                                                                      \
        output[idy * output_width + get_group_id (0)] = cache[ly * group_width];                        \
    }                                                                                                   \
}

#define DEFINE_PARALLEL_KERNEL_1(red_operation, sum_operation, default_value)                           \
kernel void reduce_1##red_operation (global float *input,                                               \
                                     global float *output,                                              \
                                     global float *param,                                               \
                                     local float *cache,                                                \
                                     const int width,                                                   \
                                     const int height,                                                  \
                                     const int input_width,                                             \
                                     const int output_width,                                            \
                                     const int pixels_per_thread)                                       \
{                                                                                                       \
    int lx = get_local_id (0);                                                                          \
    int ly = get_local_id (1);                                                                          \
    int idx = get_global_id (0);                                                                        \
    int group_width = get_local_size (0);                                                               \
    int group_height = get_local_size (1);                                                              \
    int idy = get_group_id (1) * group_height * pixels_per_thread + ly;                                 \
    int max_i = min (pixels_per_thread, (height - idy - 1) / group_height + 1);                         \
    float value = default_value;                                                                        \
                                                                                                        \
    if (idy >= height || idx >= width) {                                                                \
        cache[ly * group_width + lx] = default_value;                                                   \
        return;                                                                                         \
    }                                                                                                   \
                                                                                                        \
    for (int i = 0; i < max_i; i++) {                                                                   \
        value = red_operation (value,                                                                   \
                               input[(idy + i * group_height) * input_width + idx],                     \
                               param[idx]);                                                             \
    }                                                                                                   \
                                                                                                        \
    cache[ly * group_width + lx] = value;                                                               \
    barrier (CLK_LOCAL_MEM_FENCE);                                                                      \
                                                                                                        \
    for (int block = group_height >> 1; block > 0; block >>= 1) {                                       \
        if (ly < block) {                                                                               \
            cache[ly * group_width + lx] = sum_operation (cache[ly * group_width + lx],                 \
                                                          cache[(ly + block) * group_width + lx],       \
                                                          0);                                           \
        }                                                                                               \
        barrier (CLK_LOCAL_MEM_FENCE);                                                                  \
    }                                                                                                   \
                                                                                                        \
    if (ly == 0) {                                                                                      \
        output[get_group_id (1) * width + idx] = cache[lx];                                             \
    }                                                                                                   \
}

DEFINE_PARALLEL_KERNEL(M_SUM, M_SUM, 0.0f)
DEFINE_PARALLEL_KERNEL(M_SQUARE, M_SUM, 0.0f)
DEFINE_PARALLEL_KERNEL(M_CUBE, M_SUM, 0.0f)
DEFINE_PARALLEL_KERNEL(M_QUADRATE, M_SUM, 0.0f)
DEFINE_PARALLEL_KERNEL(M_MIN, M_MIN, INFINITY)
DEFINE_PARALLEL_KERNEL(M_MAX, M_MAX, -INFINITY)
DEFINE_PARALLEL_KERNEL_0(M_SUM, M_SUM, 0.0f)
DEFINE_PARALLEL_KERNEL_0(M_SQUARE, M_SUM, 0.0f)
DEFINE_PARALLEL_KERNEL_0(M_CUBE, M_SUM, 0.0f)
DEFINE_PARALLEL_KERNEL_0(M_QUADRATE, M_SUM, 0.0f)
DEFINE_PARALLEL_KERNEL_0(M_MIN, M_MIN, INFINITY)
DEFINE_PARALLEL_KERNEL_0(M_MAX, M_MAX, -INFINITY)
DEFINE_PARALLEL_KERNEL_1(M_SUM, M_SUM, 0.0f)
DEFINE_PARALLEL_KERNEL_1(M_SQUARE, M_SUM, 0.0f)
DEFINE_PARALLEL_KERNEL_1(M_CUBE, M_SUM, 0.0f)
DEFINE_PARALLEL_KERNEL_1(M_QUADRATE, M_SUM, 0.0f)
DEFINE_PARALLEL_KERNEL_1(M_MIN, M_MIN, INFINITY)
DEFINE_PARALLEL_KERNEL_1(M_MAX, M_MAX, -INFINITY)


kernel void parallel_sum_2D (global float *input,  
                             global float *output,
                             local float *cache,
                             const int offset,
                             const int width,
                             const int roi_width,
                             const int roi_height)
{
    int lid = get_local_id (0);
    int idx = get_global_id (0);
    int idy = get_global_id (1);
    int global_size_y = get_global_size (1);
    int block;
    float tmp = 0.0f;

    /* Load more pixels per work item to keep the GPU busy. */
    while (idy < roi_height) {
        tmp += idx < roi_width ? input[idy * width + idx + offset] : 0.0f;
        idy += global_size_y;
    }
    cache[lid] = tmp;
    barrier (CLK_LOCAL_MEM_FENCE);

    /* Parallel sum, every work item adds its own cached value plus the one's a
     * block further with block being halved in every iteration. */
    for (block = get_local_size (0) >> 1; block > 0; block >>= 1) {
        if (lid < block) {
            cache[lid] += cache[lid + block];
        }
        barrier (CLK_LOCAL_MEM_FENCE);
    }

    /* First work item in a work group stores the computed value, thus we have
     * to sum the computed values of each work group on host. */
    if (lid == 0) {
        output[get_group_id (1) * get_num_groups (0) + get_group_id (0)] = cache[0];
    }
}
