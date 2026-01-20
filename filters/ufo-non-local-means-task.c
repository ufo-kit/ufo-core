/*
 * Copyright (C) 2011-2015 Karlsruhe Institute of Technology
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
#include "config.h"

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include <math.h>

#include "ufo-non-local-means-task.h"
#include "common/ufo-math.h"
#include "common/ufo-addressing.h"
#include "common/ufo-common.h"

#define PIXELS_PER_THREAD 4

struct _UfoNonLocalMeansTaskPrivate {
    guint search_radius;
    guint patch_radius;
    gsize max_work_group_size, cropped_size[2], padded_size[2];
    gint padding;
    gfloat h;
    gfloat sigma;
    gboolean use_window;
    gboolean fast;
    gboolean estimate_sigma;
    cl_kernel kernel, mse_kernel, cumsum_kernel, spread_kernel, transpose_kernel, weight_kernel, divide_kernel,
              convolution_kernel, sum_kernel;
    cl_sampler sampler;
    cl_context context;
    cl_mem window_mem, group_sums, aux_mem[2], weights_mem;
    AddressingMode addressing_mode;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoNonLocalMeansTask, ufo_non_local_means_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_NON_LOCAL_MEANS_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_NON_LOCAL_MEANS_TASK, UfoNonLocalMeansTaskPrivate))

enum {
    PROP_0,
    PROP_SEARCH_RADIUS,
    PROP_PATCH_RADIUS,
    PROP_H,
    PROP_SIGMA,
    PROP_WINDOW,
    PROP_FAST,
    PROP_ESTIMATE_SIGMA,
    PROP_ADDRESSING_MODE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static gint
compute_cumsum_local_width (UfoNonLocalMeansTaskPrivate *priv)
{
    gdouble integer;
    gint local_width;

    /* Compute global and local dimensions for the cumsum kernel */
    /* First make sure local_width is a power of 2 */
    local_width = (gint) ufo_math_compute_closest_smaller_power_of_2 (priv->max_work_group_size);
    if (local_width > 4) {
        /* Empirically determined value on NVIDIA cards */
        local_width /= 4;
    }
    /* *local_width* is the minimum of the power-of-two-padded minimum of
     * (width, height) and the desired local width.
     */
    modf (log2 (MIN (priv->padded_size[0], priv->padded_size[1]) - 1.0f), &integer);
    local_width = (gsize) MIN (local_width, pow (2, 1 + ((gint) integer)));

    return local_width;
}

static void
release_gaussian_window (UfoNonLocalMeansTaskPrivate *priv)
{
    if (priv->window_mem) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->window_mem));
        priv->window_mem = NULL;
    }
}

static void
create_gaussian_window (UfoNonLocalMeansTaskPrivate *priv)
{
    cl_int err;
    gfloat *coefficients, coefficients_sum = 0.0f, sigma;
    gsize wsize;
    gint r;

    wsize = 2 * priv->patch_radius + 1;
    r = (gint) priv->patch_radius;
    coefficients = g_malloc0 (sizeof (gfloat) * wsize * wsize);
    sigma = priv->patch_radius / 2.0f;

    for (gint y = 0; y < (gint) wsize; y++) {
        for (gint x = 0; x < (gint) wsize; x++) {
            coefficients[y * wsize + x] = exp (- ((x - r) * (x - r) + (y - r) * (y - r)) / (sigma * sigma * 2.0f));
            coefficients_sum += coefficients[y * wsize + x];
        }
    }
    for (guint i = 0; i < wsize * wsize; i++) {
        coefficients[i] /= coefficients_sum;
    }

    priv->window_mem = clCreateBuffer (priv->context,
                                       CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                       sizeof (cl_float) * wsize * wsize,
                                       coefficients,
                                       &err);
    UFO_RESOURCES_CHECK_CLERR (err);
    g_free (coefficients);
}

static void
release_fast_buffers (UfoNonLocalMeansTaskPrivate *priv)
{
    if (priv->group_sums) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->group_sums));
        priv->group_sums = NULL;
    }
    if (priv->weights_mem) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->weights_mem));
        priv->weights_mem = NULL;
    }
    for (gint i = 0; i < 2; i++) {
        if (priv->aux_mem[i]) {
            UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->aux_mem[i]));
            priv->aux_mem[i] = NULL;
        }
    }
}

static void
create_fast_buffers (UfoNonLocalMeansTaskPrivate *priv)
{
    cl_int err;
    gint local_width = compute_cumsum_local_width (priv);

    priv->group_sums = clCreateBuffer (priv->context,
                                     CL_MEM_READ_WRITE,
                                     sizeof (cl_float) * local_width * MAX (priv->padded_size[0], priv->padded_size[1]),
                                     NULL,
                                     &err);
    UFO_RESOURCES_CHECK_CLERR (err);

    priv->weights_mem = clCreateBuffer (priv->context,
                                        CL_MEM_READ_WRITE,
                                        sizeof (cl_float) * priv->cropped_size[0] * priv->cropped_size[1],
                                        NULL,
                                        &err);
    for (gint i = 0; i < 2; i++) {
        priv->aux_mem[i] = clCreateBuffer (priv->context,
                                           CL_MEM_READ_WRITE,
                                           sizeof (cl_float) * priv->padded_size[0] * priv->padded_size[1],
                                           NULL,
                                           &err);
        UFO_RESOURCES_CHECK_CLERR (err);
    }
    UFO_RESOURCES_CHECK_CLERR (err);
}

static void
transpose (UfoNonLocalMeansTaskPrivate *priv,
           cl_command_queue cmd_queue,
           UfoProfiler *profiler,
           cl_mem in_mem,
           cl_mem out_mem,
           const gint width,
           const gint height)
{
    gsize global_size[2];
    gsize local_size[2] = {32, 32 / PIXELS_PER_THREAD};
    static gint iteration_number = 0;

    while (local_size[0] * local_size[1] > priv->max_work_group_size) {
        local_size[0] /= 2;
        local_size[1] /= 2;
    }
    global_size[0] = ((width - 1) / local_size[0] + 1) * local_size[0];
    global_size[1] = ((height - 1) / local_size[1] / PIXELS_PER_THREAD + 1) * local_size[1];
    if (!iteration_number) {
        g_debug ("Image size: %d x %d", width, height);
        g_debug ("Transpose global work group size: %lu x %lu", global_size[0], global_size[1]);
        g_debug ("Transpose local work group size: %lu x %lu", local_size[0], local_size[1]);
        iteration_number++;
    }

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->transpose_kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->transpose_kernel, 1, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->transpose_kernel, 2,
                               (local_size[0] + 1) * local_size[1] * PIXELS_PER_THREAD * sizeof (cl_float), NULL));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->transpose_kernel, 3, sizeof (cl_int), &width));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->transpose_kernel, 4, sizeof (cl_int), &height));
    ufo_profiler_call (profiler, cmd_queue, priv->transpose_kernel, 2, global_size, local_size);
}

static void
compute_cumsum (UfoNonLocalMeansTaskPrivate *priv,
                cl_command_queue cmd_queue,
                UfoProfiler *profiler,
                cl_mem in_mem,
                cl_mem out_mem,
                const gint width,
                const gint height)
{
    gint local_width, num_groups, num_group_iterations, one = 1;
    gsize cumsum_global_size[2], block_sums_global_size[2], spread_global_size[2],
          local_size[2], spread_local_size[2];
    gsize cache_size;
    static gint iteration_number = 0;

    local_width = compute_cumsum_local_width (priv);
    /* Number of groups we need to process *width* pixels. If it is more than
     * *local_width* then use only that number and every group will process more
     * successive blocks given by group size. This is necessary in order to
     * avoid recursion in the spreading phase of the group sums. The local cache
     * memory is limited to *local_width*, every group stores its sum into an
     * auxiliary buffer, which is then also summed (only one iteration needed,
     * because there is only one group in the auxiliary buffer since we limit
     * the number of groups to *local_width*.
     * This is not be the final number of groups, it's just used to compute the
     * number of iterations of every group.
     */
    num_groups = MIN (local_width, UFO_MATH_NUM_CHUNKS (width, local_width));
    /* Number of iterations of every group is given by the number of pixels
     * divided by the number of pixels *num_groups* can process. */
    num_group_iterations = UFO_MATH_NUM_CHUNKS (width, local_width * num_groups);
    /* Finally, the real number of groups is given by the number of pixels
     * divided by the group size and the number of group iterations. */
    num_groups = UFO_MATH_NUM_CHUNKS (width, num_group_iterations * local_width);

    /* Cache size must be larger by *local_size* / 16 because of the bank
     * conflicts avoidance. Additionally, +1 is needed because of the shifted
     * access to the local memory.
     */
    cache_size = sizeof (cl_float) * (local_width + UFO_MATH_NUM_CHUNKS (local_width, 16) + 1);
    cumsum_global_size[0] = num_groups * local_width / 2;
    cumsum_global_size[1] = height;
    block_sums_global_size[0] = local_width / 2;
    block_sums_global_size[1] = height;
    spread_global_size[0] = (num_groups - 1) * local_width;
    spread_global_size[1] = height;
    local_size[0] = local_width / 2;
    local_size[1] = 1;
    spread_local_size[0] = local_width;
    spread_local_size[1] = 1;
    if (!iteration_number) {
        g_debug ("           width: %d", width);
        g_debug ("     local width: %d", local_width);
        g_debug ("      num groups: %d", num_groups);
        g_debug ("group iterations: %d", num_group_iterations);
        g_debug ("     kernel dims: %lu %lu %lu %lu", cumsum_global_size[0], cumsum_global_size[1], local_size[0], local_size[1]);
        g_debug ("      cache size: %lu", cache_size);
        iteration_number++;
    }

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->cumsum_kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->cumsum_kernel, 1, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->cumsum_kernel, 2, sizeof (cl_mem), &priv->group_sums));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->cumsum_kernel, 3, cache_size, NULL));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->cumsum_kernel, 4, sizeof (gint), &num_group_iterations));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->cumsum_kernel, 5, sizeof (gint), &width));
    ufo_profiler_call (profiler, cmd_queue, priv->cumsum_kernel, 2, cumsum_global_size, local_size);

    if (num_groups > 1) {
        /* If there are more than one group, we need to spread the partial sums
         * of the groups to successive groups. First compute the sums of the
         * groups and then spread them to respective pixels in them. Because of
         * our choice of number of iterations per group above, the partial sums
         * have to be summed only once without recursion.
         */
        /* First sum the partial sums. */
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->cumsum_kernel, 0, sizeof (cl_mem), &priv->group_sums));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->cumsum_kernel, 1, sizeof (cl_mem), &priv->group_sums));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->cumsum_kernel, 2, sizeof (cl_mem), NULL));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->cumsum_kernel, 3, cache_size, NULL));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->cumsum_kernel, 4, sizeof (gint), &one));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->cumsum_kernel, 5, sizeof (gint), &local_width));
        ufo_profiler_call (profiler, cmd_queue, priv->cumsum_kernel, 2, block_sums_global_size, local_size);

        /* Spread them across all pixels. */
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->spread_kernel, 0, sizeof (cl_mem), &out_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->spread_kernel, 1, sizeof (cl_mem), &priv->group_sums));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->spread_kernel, 2, sizeof (gint), &num_group_iterations));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->spread_kernel, 3, sizeof (gint), &width));
        ufo_profiler_call (profiler, cmd_queue, priv->spread_kernel, 2, spread_global_size, spread_local_size);
    }
}

static void
compute_sdx (UfoNonLocalMeansTaskPrivate *priv,
             cl_command_queue cmd_queue,
             UfoProfiler *profiler,
             cl_mem in_mem,
             cl_mem out_mem,
             const gint dx,
             const gint dy)
{
    gfloat var = priv->sigma * priv->sigma;

    /* First compute the shifted MSE */
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->mse_kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->mse_kernel, 1, sizeof (cl_mem), &priv->aux_mem[0]));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->mse_kernel, 2, sizeof (cl_sampler), &priv->sampler));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->mse_kernel, 3, sizeof (gint), &dx));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->mse_kernel, 4, sizeof (gint), &dy));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->mse_kernel, 5, sizeof (gint), &priv->padding));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->mse_kernel, 6, sizeof (gint), &var));
    ufo_profiler_call (profiler, cmd_queue, priv->mse_kernel, 2, priv->padded_size, NULL);

    /* Horizontal cumsum and transposition */
    compute_cumsum (priv, cmd_queue, profiler, priv->aux_mem[0], priv->aux_mem[1], priv->padded_size[0], priv->padded_size[1]);
    transpose (priv, cmd_queue, profiler, priv->aux_mem[1], priv->aux_mem[0],
               priv->padded_size[0], priv->padded_size[1]);

    /* 2D cumsum is separable, so compute the horizontal cumsum of the transposed horizontally cumsummed image */
    compute_cumsum (priv, cmd_queue, profiler, priv->aux_mem[0], priv->aux_mem[1], priv->padded_size[1], priv->padded_size[0]);
    transpose (priv, cmd_queue, profiler, priv->aux_mem[1], priv->aux_mem[0],
               priv->padded_size[1], priv->padded_size[0]);
}

static void
process_shift (UfoNonLocalMeansTaskPrivate *priv,
                cl_command_queue cmd_queue,
                UfoProfiler *profiler,
                cl_mem input_image,
                cl_mem integral_mem,
                cl_mem out_mem,
                const gint dx,
                const gint dy)
{
    gint patch_size = 2 * priv->patch_radius + 1;
    gfloat coeff = 1.0f / (priv->h * priv->h * patch_size * patch_size);
    gint is_conjugate = 0;

    /* Compute r(x) = w(x) * f(x + dx) */
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 0, sizeof (cl_mem), &input_image));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 1, sizeof (cl_mem), &integral_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 2, sizeof (cl_mem), &priv->weights_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 3, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 4, sizeof (cl_sampler), &priv->sampler));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 5, sizeof (gint), &dx));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 6, sizeof (gint), &dy));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 7, sizeof (gint), &priv->patch_radius));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 8, sizeof (gint), &priv->padding));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 9, sizeof (gfloat), &coeff));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 10, sizeof (gint), &is_conjugate));
    ufo_profiler_call (profiler, cmd_queue, priv->weight_kernel, 2, priv->cropped_size, NULL);

    if (dx != 0 || dy != 0) {
        /* Compute r(x + dx) = w(x + dx) * f(x), which cannot be computed at
         * once in the above kernel call because one work item would write to
         * two global memory locations, thus creating a race condition. */
        is_conjugate = 1;
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 0, sizeof (cl_mem), &input_image));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 1, sizeof (cl_mem), &integral_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 2, sizeof (cl_mem), &priv->weights_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 3, sizeof (cl_mem), &out_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 4, sizeof (cl_sampler), &priv->sampler));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 5, sizeof (gint), &dx));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 6, sizeof (gint), &dy));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 7, sizeof (gint), &priv->patch_radius));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 8, sizeof (gint), &priv->padding));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 9, sizeof (gfloat), &coeff));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->weight_kernel, 10, sizeof (gint), &is_conjugate));
        ufo_profiler_call (profiler, cmd_queue, priv->weight_kernel, 2, priv->cropped_size, NULL);
    }
}

static void
compute_nlm_fast (UfoNonLocalMeansTaskPrivate *priv,
                  cl_command_queue cmd_queue,
                  UfoProfiler *profiler,
                  cl_mem in_mem,
                  cl_mem out_mem)
{
    gint dx, dy, sr;
    sr = (gint) priv->search_radius;

    /* Every pixel computes its own result and spreads it to the pixel y + dy,
     * thus we start at dy = 0 every pixel gets its dy < 0 value from the pixel
     * y - dy. For dy = 0, compute only pixels to the right, so the negative
     * values are obtained from the pixels to the left (analogous to the dy
     * case).
     */
    for (dy = 0; dy < sr + 1; dy++) {
        for (dx = dy == 0 ? 0 : -sr; dx < sr + 1; dx++) {
            compute_sdx (priv, cmd_queue, profiler, in_mem, out_mem, dx, dy);
            process_shift (priv, cmd_queue, profiler, in_mem, priv->aux_mem[0], out_mem, dx, dy);
        }
    }
    /* Now we have the sum of results and weights, divide them to get the
     * result.
     */
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->divide_kernel, 0, sizeof (cl_mem), &priv->weights_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->divide_kernel, 1, sizeof (cl_mem), &out_mem));
    ufo_profiler_call (profiler, cmd_queue, priv->divide_kernel, 2, priv->cropped_size, NULL);
}

UfoNode *
ufo_non_local_means_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_NON_LOCAL_MEANS_TASK, NULL));
}

static void
ufo_non_local_means_task_setup (UfoTask *task,
                                UfoResources *resources,
                                GError **error)
{
    UfoNonLocalMeansTaskPrivate *priv;
    cl_int err;

    priv = UFO_NON_LOCAL_MEANS_TASK_GET_PRIVATE (task);
    if (priv->fast) {
        priv->mse_kernel = ufo_resources_get_kernel (resources, "nlm.cl", "compute_shifted_mse", NULL, error);
        priv->cumsum_kernel = ufo_resources_get_kernel (resources, "cumsum.cl", "cumsum", NULL, error);
        priv->spread_kernel = ufo_resources_get_kernel (resources, "cumsum.cl", "spread_block_sums", NULL, error);
        priv->transpose_kernel = ufo_resources_get_kernel (resources, "transpose.cl", "transpose_shared", NULL, error);
        priv->weight_kernel = ufo_resources_get_kernel (resources, "nlm.cl", "process_shift", NULL, error);
        priv->divide_kernel = ufo_resources_get_kernel (resources, "nlm.cl", "divide_inplace", NULL, error);
    } else {
        priv->kernel = ufo_resources_get_kernel (resources, "nlm.cl", "nlm_noise_reduction", NULL, error);
    }
    priv->convolution_kernel = ufo_resources_get_kernel (resources, "estimate-noise.cl", "convolve_abs_laplacian_diff", NULL, error);
    priv->sum_kernel = ufo_resources_get_kernel (resources, "reductor.cl", "reduce_M_SUM", NULL, error);

    priv->window_mem = NULL;
    priv->group_sums = NULL;
    priv->weights_mem = NULL;
    for (gint i = 0; i < 2; i++) {
        priv->aux_mem[i] = NULL;
    }

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
    }
    if (priv->mse_kernel) {
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->mse_kernel), error);
    }
    if (priv->cumsum_kernel) {
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->cumsum_kernel), error);
    }
    if (priv->spread_kernel) {
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->spread_kernel), error);
    }
    if (priv->transpose_kernel) {
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->transpose_kernel), error);
    }
    if (priv->weight_kernel) {
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->weight_kernel), error);
    }
    if (priv->divide_kernel) {
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->divide_kernel), error);
    }
    if (priv->convolution_kernel) {
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->convolution_kernel), error);
    }
    if (priv->sum_kernel) {
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->sum_kernel), error);
    }

    priv->context = ufo_resources_get_context (resources);
    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);

    priv->sampler = clCreateSampler (priv->context,
                                     (cl_bool) TRUE,
                                     priv->addressing_mode,
                                     CL_FILTER_NEAREST,
                                     &err);
    UFO_RESOURCES_CHECK_CLERR (err);
}

static void
ufo_non_local_means_task_get_requisition (UfoTask *task,
                                          UfoBuffer **inputs,
                                          UfoRequisition *requisition,
                                          GError **error)
{
    UfoNonLocalMeansTaskPrivate *priv = UFO_NON_LOCAL_MEANS_TASK_GET_PRIVATE (task);
    UfoGpuNode *node;
    GValue *max_work_group_size_gvalue;

    ufo_buffer_get_requisition (inputs[0], requisition);

    if (priv->max_work_group_size == 0) {
        node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
        max_work_group_size_gvalue = ufo_gpu_node_get_info (node, UFO_GPU_NODE_INFO_MAX_WORK_GROUP_SIZE);
        priv->max_work_group_size = g_value_get_ulong (max_work_group_size_gvalue);
        g_value_unset (max_work_group_size_gvalue);
    }
    if (!priv->fast && priv->use_window && !priv->window_mem) {
        create_gaussian_window (priv);
    }
    if (priv->cropped_size[0] != requisition->dims[0] || priv->cropped_size[1] != requisition->dims[1]) {
        priv->cropped_size[0] = requisition->dims[0];
        priv->cropped_size[1] = requisition->dims[1];
        if (priv->fast) {
            priv->padding = priv->search_radius + priv->patch_radius + 1;
            priv->padded_size[0] = priv->cropped_size[0] + 2 * priv->padding;
            priv->padded_size[1] = priv->cropped_size[1] + 2 * priv->padding;
            if (priv->group_sums) {
                /* Buffers exist, which means size changed, release first. */
                release_fast_buffers (priv);
            }
            create_fast_buffers (priv);
        }
    }
}

static guint
ufo_non_local_means_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_non_local_means_task_get_num_dimensions (UfoTask *task,
                                             guint input)
{
    return 2;
}

static UfoTaskMode
ufo_non_local_means_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_non_local_means_task_process (UfoTask *task,
                                  UfoBuffer **inputs,
                                  UfoBuffer *output,
                                  UfoRequisition *requisition)
{
    UfoNonLocalMeansTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    UfoRequisition in_req;
    cl_command_queue cmd_queue;
    cl_mem in_mem;
    cl_mem out_mem;
    gfloat h, var, estimated_sigma;
    gfloat fill_pattern = 0.0f;
    guint num_processed;

    priv = UFO_NON_LOCAL_MEANS_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    in_mem = ufo_buffer_get_device_image (inputs[0], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_buffer_get_requisition (inputs[0], &in_req);
    g_object_get (task, "num_processed", &num_processed, NULL);

    /* Estimate sigma only from the first image in the stream in order to
     * keep the results consistent.
     */
    if (num_processed == 0 && (priv->h <= 0.0f || priv->estimate_sigma)) {
        /* Use out_mem for the convolution, it's not necessary after the
         * computation and can be re-used by the de-noising itself.
         */
        estimated_sigma = ufo_common_estimate_sigma (priv->convolution_kernel,
                                                     priv->sum_kernel,
                                                     cmd_queue,
                                                     priv->sampler,
                                                     profiler,
                                                     in_mem,
                                                     out_mem,
                                                     priv->max_work_group_size,
                                                     priv->cropped_size);
        g_debug ("Estimated sigma: %g", estimated_sigma);
        if (priv->h <= 0.0f) {
            priv->h = estimated_sigma;
        }
        if (priv->estimate_sigma) {
            priv->sigma = estimated_sigma;
        }
    }
    h = 1 / priv->h / priv->h;
    var = priv->sigma * priv->sigma;

    if (priv->fast) {
        UFO_RESOURCES_CHECK_CLERR (clEnqueueFillBuffer (cmd_queue, out_mem, &fill_pattern, sizeof (gfloat), 0,
                                                        sizeof (gfloat) * priv->cropped_size[0] * priv->cropped_size[1],
                                                        0, NULL, NULL));
        UFO_RESOURCES_CHECK_CLERR (clEnqueueFillBuffer (cmd_queue, priv->weights_mem, &fill_pattern, sizeof (gfloat), 0,
                                                        sizeof (gfloat) * priv->cropped_size[0] * priv->cropped_size[1],
                                                        0, NULL, NULL));
        compute_nlm_fast (priv, cmd_queue, profiler, in_mem, out_mem);
    } else {
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &in_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &out_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (cl_sampler), &priv->sampler));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 3, sizeof (guint), &priv->search_radius));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 4, sizeof (guint), &priv->patch_radius));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 5, sizeof (gfloat), &h));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 6, sizeof (gfloat), &var));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 7, sizeof (cl_mem), &priv->window_mem));
        ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, requisition->dims, NULL);
    }

    return TRUE;
}


static void
ufo_non_local_means_task_set_property (GObject *object,
                                       guint property_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
    UfoNonLocalMeansTaskPrivate *priv = UFO_NON_LOCAL_MEANS_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_SEARCH_RADIUS:
            priv->search_radius = g_value_get_uint (value);
            break;
        case PROP_PATCH_RADIUS:
            priv->patch_radius = g_value_get_uint (value);
            break;
        case PROP_H:
            priv->h = g_value_get_float (value);
            break;
        case PROP_SIGMA:
            priv->sigma = g_value_get_float (value);
            break;
        case PROP_WINDOW:
            priv->use_window = g_value_get_boolean (value);
            break;
        case PROP_FAST:
            priv->fast = g_value_get_boolean (value);
            break;
        case PROP_ESTIMATE_SIGMA:
            priv->estimate_sigma = g_value_get_boolean (value);
            break;
        case PROP_ADDRESSING_MODE:
            priv->addressing_mode = g_value_get_enum (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_non_local_means_task_get_property (GObject *object,
                                       guint property_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
    UfoNonLocalMeansTaskPrivate *priv = UFO_NON_LOCAL_MEANS_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_SEARCH_RADIUS:
            g_value_set_uint (value, priv->search_radius);
            break;
        case PROP_PATCH_RADIUS:
            g_value_set_uint (value, priv->patch_radius);
            break;
        case PROP_H:
            g_value_set_float (value, priv->h);
            break;
        case PROP_SIGMA:
            g_value_set_float (value, priv->sigma);
            break;
        case PROP_WINDOW:
            g_value_set_boolean (value, priv->use_window);
            break;
        case PROP_FAST:
            g_value_set_boolean (value, priv->fast);
            break;
        case PROP_ESTIMATE_SIGMA:
            g_value_set_boolean (value, priv->estimate_sigma);
            break;
        case PROP_ADDRESSING_MODE:
            g_value_set_enum (value, priv->addressing_mode);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_non_local_means_task_finalize (GObject *object)
{
    UfoNonLocalMeansTaskPrivate *priv;

    priv = UFO_NON_LOCAL_MEANS_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }
    if (priv->mse_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->mse_kernel));
        priv->mse_kernel = NULL;
    }
    if (priv->cumsum_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->cumsum_kernel));
        priv->cumsum_kernel = NULL;
    }
    if (priv->spread_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->spread_kernel));
        priv->spread_kernel = NULL;
    }
    if (priv->transpose_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->transpose_kernel));
        priv->transpose_kernel = NULL;
    }
    if (priv->weight_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->weight_kernel));
        priv->weight_kernel = NULL;
    }
    if (priv->divide_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->divide_kernel));
        priv->divide_kernel = NULL;
    }
    if (priv->convolution_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->convolution_kernel));
        priv->convolution_kernel = NULL;
    }
    if (priv->sum_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->sum_kernel));
        priv->sum_kernel = NULL;
    }
    if (priv->sampler) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseSampler (priv->sampler));
        priv->sampler = NULL;
    }
    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }
    release_gaussian_window (priv);
    release_fast_buffers (priv);

    G_OBJECT_CLASS (ufo_non_local_means_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_non_local_means_task_setup;
    iface->get_num_inputs = ufo_non_local_means_task_get_num_inputs;
    iface->get_num_dimensions = ufo_non_local_means_task_get_num_dimensions;
    iface->get_mode = ufo_non_local_means_task_get_mode;
    iface->get_requisition = ufo_non_local_means_task_get_requisition;
    iface->process = ufo_non_local_means_task_process;
}

static void
ufo_non_local_means_task_class_init (UfoNonLocalMeansTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_non_local_means_task_set_property;
    oclass->get_property = ufo_non_local_means_task_get_property;
    oclass->finalize = ufo_non_local_means_task_finalize;

    properties[PROP_SEARCH_RADIUS] =
        g_param_spec_uint ("search-radius",
            "Search radius in pixels",
            "Search radius in pixels",
            1, 8192, 10,
            G_PARAM_READWRITE);

    properties[PROP_PATCH_RADIUS] =
        g_param_spec_uint ("patch-radius",
            "Patch radius in pixels",
            "Patch radius in pixels",
            1, 100, 3,
            G_PARAM_READWRITE);

    properties[PROP_H] =
        g_param_spec_float ("h",
            "Smoothing control parameter, should be around noise standard deviation or slightly less",
            "Smoothing control parameter, should be around noise standard deviation or slightly less",
            0.0f, G_MAXFLOAT, 0.1f,
            G_PARAM_READWRITE);

    properties[PROP_SIGMA] =
        g_param_spec_float ("sigma",
            "Noise standard deviation",
            "Noise standard deviation",
            0.0f, G_MAXFLOAT, 0.0f,
            G_PARAM_READWRITE);

    properties[PROP_WINDOW] =
        g_param_spec_boolean ("window",
            "Use Gaussian window by computing patch weights",
            "Use Gaussian window by computing patch weights",
            TRUE,
            G_PARAM_READWRITE);

    properties[PROP_FAST] =
        g_param_spec_boolean ("fast",
            "Use a faster version of the algorithm",
            "Use a faster version of the algorithm",
            TRUE,
            G_PARAM_READWRITE);

    properties[PROP_ESTIMATE_SIGMA] =
        g_param_spec_boolean ("estimate-sigma",
            "Estimate noise standard deviation",
            "Estimate noise standard deviation",
            FALSE,
            G_PARAM_READWRITE);

    properties[PROP_ADDRESSING_MODE] =
        g_param_spec_enum ("addressing-mode",
            "Outlier treatment (\"none\", \"clamp\", \"clamp_to_edge\", \"repeat\", \"mirrored_repeat\")",
            "Outlier treatment (\"none\", \"clamp\", \"clamp_to_edge\", \"repeat\", \"mirrored_repeat\")",
            g_enum_register_static ("ufo_nlm_addressing_mode", addressing_values),
            CL_ADDRESS_MIRRORED_REPEAT,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoNonLocalMeansTaskPrivate));
}

static void
ufo_non_local_means_task_init(UfoNonLocalMeansTask *self)
{
    self->priv = UFO_NON_LOCAL_MEANS_TASK_GET_PRIVATE(self);

    self->priv->search_radius = 10;
    self->priv->patch_radius = 3;
    self->priv->cropped_size[0] = 0;
    self->priv->cropped_size[1] = 0;
    self->priv->padded_size[0] = 0;
    self->priv->padded_size[1] = 0;
    self->priv->padding = -1;
    self->priv->h = 0.0f;
    self->priv->sigma = 0.0f;
    self->priv->max_work_group_size = 0;
    self->priv->use_window = TRUE;
    self->priv->addressing_mode = CL_ADDRESS_MIRRORED_REPEAT;
    self->priv->fast = TRUE;
    self->priv->estimate_sigma = FALSE;
}
