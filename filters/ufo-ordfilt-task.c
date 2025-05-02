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
#include "config.h"

#ifdef __APPLE__
# include <OpenCL/cl.h>
#else
# include <CL/cl.h>
#endif

#include <limits.h>
#include <glib.h>

#include "ufo-ordfilt-task.h"
#include "ufo-priv.h"

struct _UfoOrdfiltTaskPrivate {
    cl_kernel k_bitonic_ordfilt;
    cl_kernel k_load_elements_from_patern;
    size_t max_alloc_size;
    gpointer context;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoOrdfiltTask, ufo_ordfilt_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_ORDFILT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_ORDFILT_TASK, UfoOrdfiltTaskPrivate))

UfoNode *
ufo_ordfilt_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_ORDFILT_TASK, NULL));
}

static void
get_max_alloc_size (UfoResources *resources, UfoOrdfiltTaskPrivate *priv)
{
    priv->max_alloc_size = G_MAXSIZE;
    GList *devices = ufo_resources_get_devices (resources);
    GList *it;

    g_list_for (devices, it) {
        cl_device_id device = (cl_device_id) it->data;
        size_t byte_count = 0;
        size_t max_alloc_size = 0;
        clGetDeviceInfo (device, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof (size_t), &max_alloc_size, &byte_count);
        g_assert (sizeof (size_t) == byte_count);

        if (max_alloc_size < priv->max_alloc_size)
            priv->max_alloc_size = max_alloc_size;
    }
}

static void
ufo_ordfilt_task_setup (UfoTask *task,
                       UfoResources *resources,
                       GError **error)
{
    UfoOrdfiltTaskPrivate *priv;

    priv = UFO_ORDFILT_TASK_GET_PRIVATE (task);
    priv->context = ufo_resources_get_context (resources);
    get_max_alloc_size (resources, priv);

    priv->k_bitonic_ordfilt = ufo_resources_get_kernel (resources, "ordfilt.cl", "bitonic_ordfilt", NULL, error);

    if (priv->k_bitonic_ordfilt != NULL)
        UFO_RESOURCES_CHECK_CLERR (clRetainKernel (priv->k_bitonic_ordfilt));

    priv->k_load_elements_from_patern = ufo_resources_get_kernel (resources, "ordfilt.cl", "load_elements_from_pattern", NULL, error);

    if (priv->k_load_elements_from_patern != NULL)
        UFO_RESOURCES_CHECK_CLERR (clRetainKernel (priv->k_load_elements_from_patern));
}

static void
ufo_ordfilt_task_get_requisition (UfoTask *task,
                                  UfoBuffer **inputs,
                                  UfoRequisition *requisition,
                                  GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_ordfilt_task_get_num_inputs (UfoTask *task)
{
    return 2;
}

static guint
ufo_ordfilt_task_get_num_dimensions (UfoTask *task,
                                     guint input)
{
    return 2;
}

static UfoTaskMode
ufo_ordfilt_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

/* K is ordfilt kernel */
static void
launch_kernel_1D (cl_kernel k, UfoBuffer *ufo_src, UfoBuffer *ufo_dst,
                  cl_command_queue cmd_queue, size_t num_elements,
                  unsigned idx_offset, unsigned mod)
{
    cl_mem dst;
    cl_mem src;
    UfoRequisition requisition;
    size_t global_work_size[1];
    size_t local_work_size[1];

    dst = ufo_buffer_get_device_array (ufo_dst, cmd_queue);
    src = ufo_buffer_get_device_array (ufo_src, cmd_queue);

    /* Power of 2 above num_elements */
    size_t array_length = (size_t) ceil_power_of_two (num_elements);
    cl_float low_threshold = 0.25;
    cl_float high_threshold = 0.50;
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (k, 0, sizeof (cl_mem), &src));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (k, 1, sizeof (cl_mem), &dst));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (k, 2, sizeof (cl_int), &num_elements));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (k, 3, sizeof (cl_int), &array_length));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (k, 4, sizeof (cl_float), &low_threshold));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (k, 5, sizeof (cl_float), &high_threshold));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (k, 6, sizeof (cl_float) * array_length, NULL));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (k, 7, sizeof (cl_uint), &idx_offset));

    /* Launch the kernel over 1D grid */
    ufo_buffer_get_requisition(ufo_src, &requisition);

    /* buffer may have mod extra rows -> so dont do extra work */
    global_work_size[0] = requisition.dims[0] * (requisition.dims[1] - mod) *
                          (array_length / 2);
    /* Power of two below number of elements to sort */
    local_work_size[0] = array_length / 2;
    UFO_RESOURCES_CHECK_CLERR (clEnqueueNDRangeKernel (cmd_queue,
                                                       k,
                                                       1, NULL, global_work_size,
                                                       local_work_size,
                                                       0, NULL, NULL));
}

static void
launch_kernel_2D(cl_kernel kernel, UfoBuffer *ufo_src, UfoBuffer *ufo_pattern,
                 UfoBuffer *ufo_dst, cl_command_queue cmd_queue, size_t dimension,
                 size_t num_ones, unsigned height, unsigned y_offset, unsigned mod)
{
    cl_mem dst;
    cl_mem src;
    cl_mem pattern;
    UfoRequisition requisition;
    size_t global_work_size[2];
    size_t local_work_size[2];
    dst = ufo_buffer_get_device_array(ufo_dst, cmd_queue);
    src = ufo_buffer_get_device_array(ufo_src, cmd_queue);
    pattern = ufo_buffer_get_device_array(ufo_pattern, cmd_queue);

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 0, sizeof (cl_mem), &src));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 1, sizeof (cl_mem), &dst));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 2, sizeof (cl_mem), &pattern));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 3, sizeof (cl_int), &dimension));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 4, sizeof (cl_int), &num_ones));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 5, sizeof (cl_uint), &height));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 6, sizeof (cl_uint), &y_offset));

    /* Launch the kernel over 2D grid, using dst requisition which reperesents a
     * crop of the image */
    ufo_buffer_get_requisition (ufo_dst, &requisition);
    global_work_size[0] = requisition.dims[0];
    /* Buffer may have mod extra rows, don't take them into account */
    global_work_size[1] = requisition.dims[1] - mod;
    unsigned y_worker_count = 32;
    unsigned x_worker_count = 32;

    while (global_work_size[1] % y_worker_count)
        --y_worker_count;

    while (global_work_size[0] % x_worker_count)
        --x_worker_count;

    local_work_size[0] = x_worker_count; /* Multiple of image_width=1080 */
    local_work_size[1] = y_worker_count; /* Multiple of image_height=1280 */
    UFO_RESOURCES_CHECK_CLERR (clEnqueueNDRangeKernel (cmd_queue,
                                                       kernel,
                                                       2, NULL, global_work_size,
                                                       local_work_size,
                                                       0, NULL, NULL));
}

static void
get_ring_metadata(UfoBuffer *pattern, unsigned *number_ones, unsigned *radius)
{
    GValue *value;
    value = ufo_buffer_get_metadata(pattern, "number_ones");
    *number_ones = g_value_get_uint(value);

    value = ufo_buffer_get_metadata(pattern, "radius");
    *radius = g_value_get_uint(value);
}

static void
compute_ordfilt (UfoOrdfiltTaskPrivate *priv, UfoBuffer *src, UfoBuffer *pattern,
                 UfoBuffer *dst, cl_command_queue cmd_queue)
{

    UfoRequisition image_requisition;
    UfoRequisition pattern_requisition;
    ufo_buffer_get_requisition(src, &image_requisition);
    ufo_buffer_get_requisition(pattern, &pattern_requisition);

    unsigned number_ones;
    unsigned radius;
    get_ring_metadata(pattern, &number_ones, &radius);

    unsigned height = (unsigned) image_requisition.dims[1];
    unsigned width = (unsigned) image_requisition.dims[0];
    /* Tells us in how many chunks to chop image */
    unsigned iter_count = (unsigned) (1 + sizeof (float) * height * width * number_ones / (priv->max_alloc_size + 1));
    unsigned y_offset = 0;

    /* On first iteration process mod rows more, this is needed when the height
     * of image is not divisible by iter_count */
    unsigned mod = (unsigned) (image_requisition.dims[1] % iter_count);

    UfoRequisition requisition = {
        .n_dims = 3,
        .dims[0] = image_requisition.dims[0],
        .dims[1] = image_requisition.dims[1] / iter_count + mod,
        .dims[2] = number_ones,
    };
    UfoBuffer *ufo_buffer = ufo_buffer_new(&requisition, priv->context);

    /* loads surrounding number_ones pixels of each pixel in image.
     * Result in buffer */
    launch_kernel_2D (priv->k_load_elements_from_patern, src, pattern,
                      ufo_buffer, cmd_queue, pattern_requisition.dims[0],
                      number_ones, height, y_offset, 0);
    /* Create image of threshold telling how likely a pixel is a center of a
     * ring */
    launch_kernel_1D (priv->k_bitonic_ordfilt, ufo_buffer, dst, cmd_queue,
                      number_ones, width * y_offset, 0);

    for (unsigned iter = 0; iter < iter_count; ++iter) {
        /* start at mod offset, since first iteration manipulated
         * iter * height + mod rows */
        y_offset = mod + iter * (height - mod) / iter_count;
        launch_kernel_2D (priv->k_load_elements_from_patern, src, pattern,
                          ufo_buffer, cmd_queue, pattern_requisition.dims[0],
                          number_ones, height, y_offset, mod);
        launch_kernel_1D (priv->k_bitonic_ordfilt, ufo_buffer, dst, cmd_queue,
                          number_ones, width * y_offset, mod);
    }

    g_object_unref(ufo_buffer);
}

static gboolean
ufo_ordfilt_task_process (UfoTask *task,
                          UfoBuffer **inputs,
                          UfoBuffer *output,
                          UfoRequisition *requisition)
{
    UfoOrdfiltTaskPrivate *priv;
    UfoGpuNode *node;
    cl_command_queue cmd_queue;

    priv = UFO_ORDFILT_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    compute_ordfilt (priv, inputs[0], inputs[1], output, cmd_queue);

    return TRUE;
}

static void
ufo_ordfilt_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_ordfilt_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_ordfilt_task_setup;
    iface->get_num_inputs = ufo_ordfilt_task_get_num_inputs;
    iface->get_num_dimensions = ufo_ordfilt_task_get_num_dimensions;
    iface->get_mode = ufo_ordfilt_task_get_mode;
    iface->get_requisition = ufo_ordfilt_task_get_requisition;
    iface->process = ufo_ordfilt_task_process;
}

static void
ufo_ordfilt_task_class_init (UfoOrdfiltTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = ufo_ordfilt_task_finalize;

    g_type_class_add_private (gobject_class, sizeof(UfoOrdfiltTaskPrivate));
}

static void
ufo_ordfilt_task_init(UfoOrdfiltTask *self)
{
    self->priv = UFO_ORDFILT_TASK_GET_PRIVATE(self);
}
