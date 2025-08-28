/*
 * Copyright (C) 2015-2019 Karlsruhe Institute of Technology
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
#include <math.h>
#include <glib.h>
#include "ufo-math.h"
#include "ufo-common.h"

gfloat
ufo_common_estimate_sigma (cl_kernel convolution_kernel,
                           cl_kernel sum_kernel,
                           cl_command_queue cmd_queue,
                           cl_sampler sampler,
                           UfoProfiler *profiler,
                           cl_mem input_image,
                           cl_mem out_mem,
                           const gsize max_work_group_size,
                           const gsize *global_size)
{
    gsize n = global_size[0] * global_size[1];
    gsize local_size, num_groups, global_size_1D;
    gint num_group_iterations;
    gfloat *result, sum = 0.0f;
    cl_int err;
    cl_mem group_sums;
    cl_context context;
    
    clGetCommandQueueInfo (cmd_queue, CL_QUEUE_CONTEXT, sizeof (cl_context), &context, NULL);

    /* First compute the convolution of the input with the difference of
     * laplacians.
     */
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (convolution_kernel, 0, sizeof (cl_mem), &input_image));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (convolution_kernel, 1, sizeof (cl_sampler), &sampler));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (convolution_kernel, 2, sizeof (cl_mem), &out_mem));
    ufo_profiler_call (profiler, cmd_queue, convolution_kernel, 2, global_size, NULL);

    /* Now compute partial sums of the convolved image. */
    /* Compute global and local dimensions for the cumsum kernel */
    /* Make sure local_size is a power of 2 */
    local_size = ufo_math_compute_closest_smaller_power_of_2 (max_work_group_size);
    /* Number of iterations of every group is given by the number of pixels
     * divided by the number of pixels *num_groups* can process. */
    num_groups = MIN (local_size, UFO_MATH_NUM_CHUNKS (n, local_size));
    num_group_iterations = UFO_MATH_NUM_CHUNKS (n, local_size * num_groups);
    /* The real number of groups is given by the number of pixels
     * divided by the group size and the number of group iterations. */
    num_groups = UFO_MATH_NUM_CHUNKS (n, num_group_iterations * local_size);
    global_size_1D = num_groups * local_size;

    g_debug ("                 n: %lu", n);
    g_debug ("        num groups: %lu", num_groups);
    g_debug ("  group iterations: %d", num_group_iterations);
    g_debug ("kernel global size: %lu", global_size_1D);
    g_debug (" kernel local size: %lu", local_size);

    result = g_malloc0 (sizeof (cl_float) * num_groups);
    group_sums = clCreateBuffer (context,
                                 CL_MEM_READ_WRITE,
                                 sizeof (cl_float) * num_groups,
                                 NULL,
                                 &err);
    UFO_RESOURCES_CHECK_CLERR (err);

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (sum_kernel, 0, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (sum_kernel, 1, sizeof (cl_mem), &group_sums));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (sum_kernel, 2, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (sum_kernel, 3, sizeof (cl_float) * local_size, NULL));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (sum_kernel, 4, sizeof (gsize), &n));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (sum_kernel, 5, sizeof (gint), &num_group_iterations));
    ufo_profiler_call (profiler, cmd_queue, sum_kernel, 1, &global_size_1D, &local_size);

    clEnqueueReadBuffer (cmd_queue,
                         group_sums,
                         CL_TRUE,
                         0, sizeof (cl_float) * num_groups,
                         result,
                         0, NULL, NULL);
    UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (group_sums));

    /* Sum partial sums computed by the groups. */
    for (gsize i = 0; i < num_groups; i++) {
        sum += result[i];
    }
    g_free (result);

    return sqrt (G_PI_2) / (6 * (global_size[0] - 2.0f) * (global_size[1] - 2.0f)) * sum;
}
