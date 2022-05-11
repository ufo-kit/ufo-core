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
 *
 * Authored by: Alexandre Lewkowicz (lewkow_a@epita.fr)
 */
#include "config.h"

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <math.h>
#include "ufo-denoise-task.h"
#include "ufo-priv.h"


struct _UfoDenoiseTaskPrivate {
    cl_kernel k_sort_and_set;
    cl_kernel k_load_elements;
    cl_kernel k_remove_background;
    unsigned matrix_size;
    gpointer context;
    UfoResources *resources;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoDenoiseTask, ufo_denoise_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_DENOISE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_DENOISE_TASK, UfoDenoiseTaskPrivate))

enum {
    PROP_0,
    PROP_MATRIX_SIZE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_denoise_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_DENOISE_TASK, NULL));
}

static void
ufo_denoise_task_setup (UfoTask *task,
                   UfoResources *resources,
                   GError **error)
{
    UfoDenoiseTaskPrivate *priv;

    priv = UFO_DENOISE_TASK_GET_PRIVATE (task);
    priv->context = ufo_resources_get_context (resources);
    priv->resources = resources;

    priv->k_sort_and_set = ufo_resources_get_kernel (resources, "denoise.cl", "sort_and_set", NULL, error);

    if (priv->k_sort_and_set != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->k_sort_and_set), error);

    priv->k_load_elements = ufo_resources_get_kernel (resources, "denoise.cl", "load_elements", NULL, error);

    if (priv->k_load_elements != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->k_load_elements), error);

    priv->k_remove_background = ufo_resources_get_kernel (resources, "denoise.cl", "remove_background", NULL, error);

    if (priv->k_remove_background != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->k_remove_background), error);
}

static void
ufo_denoise_task_get_requisition (UfoTask *task,
                                  UfoBuffer **inputs,
                                  UfoRequisition *requisition,
                                  GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_denoise_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_denoise_task_get_num_dimensions (UfoTask *task,
                                     guint input)
{
    return 2;
}

static UfoTaskMode
ufo_denoise_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static void
launch_kernel_1D(cl_kernel kernel, UfoBuffer *ufo_src, UfoBuffer *ufo_dst,
                 cl_command_queue cmd_queue, size_t dimension)
{
    cl_mem dst;
    cl_mem src;
    UfoRequisition requisition;
    size_t global_work_size[1];
    size_t local_work_size[1];

    dst = ufo_buffer_get_device_array(ufo_dst, cmd_queue);
    src = ufo_buffer_get_device_array(ufo_src, cmd_queue);

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 0, sizeof (cl_mem),
                                               &src));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 1, sizeof (cl_mem),
                                               &dst));
    size_t num_elements = dimension * dimension;
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 2, sizeof (cl_int),
                                               &num_elements));
    /* Power of 2 above dimension * dimension */
    size_t array_length = (size_t) ceil_power_of_two (dimension * dimension);
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 3, sizeof (cl_int),
                                               &array_length));
    float threshold = (float) 0.3;
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 4, sizeof (cl_float),
                                               &threshold));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 5, sizeof (cl_float) *
                                               array_length, NULL));

    /* Launch the kernel over 1D grid */
    ufo_buffer_get_requisition(ufo_src, &requisition);
    global_work_size[0] = requisition.dims[0] * requisition.dims[1] *
                          (array_length / 2);
    /* Power of two below number of elements to sort */
    local_work_size[0] = array_length / 2;
    UFO_RESOURCES_CHECK_CLERR (clEnqueueNDRangeKernel (cmd_queue,
                                                       kernel,
                                                       1, NULL, global_work_size,
                                                       local_work_size,
                                                       0, NULL, NULL));
}

static void
get_max_work_group_size (UfoResources *resources, size_t *x_worker_count,
                         size_t * y_worker_count)
{
    *x_worker_count = G_MAXSIZE;
    GList *devices = ufo_resources_get_devices (resources);
    GList *it;
    g_list_for (devices, it) {
        cl_device_id device = (cl_device_id) it->data;
        size_t byte_count = 0;
        size_t max_group_size = 0;
        clGetDeviceInfo (device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof (size_t),
                         &max_group_size, &byte_count);
        g_assert (sizeof (size_t) == byte_count);
        if (max_group_size < *x_worker_count)
            *x_worker_count = max_group_size;
    }
    *x_worker_count = (unsigned) sqrtf((float)*x_worker_count);
    *y_worker_count = *x_worker_count;
}

static void
launch_kernel_2D(UfoDenoiseTaskPrivate *priv,
                 cl_kernel kernel, UfoBuffer *ufo_src, UfoBuffer *ufo_dst,
                 cl_command_queue cmd_queue, size_t dimension)
{
    cl_mem dst;
    cl_mem src;
    UfoRequisition requisition;
    size_t global_work_size[2];
    size_t local_work_size[2];
    dst = ufo_buffer_get_device_array(ufo_dst, cmd_queue);
    src = ufo_buffer_get_device_array(ufo_src, cmd_queue);

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 0, sizeof (cl_mem),
                                               &src));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 1, sizeof (cl_mem),
                                               &dst));
    if (dimension)
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 2, sizeof (cl_int),
                                                   &dimension));

    ufo_buffer_get_requisition(ufo_src, &requisition);
    global_work_size[0] = requisition.dims[0];
    global_work_size[1] = requisition.dims[1];
    size_t y_worker_count, x_worker_count;
    get_max_work_group_size(priv->resources, &x_worker_count, &y_worker_count);

    while (global_work_size[1] % y_worker_count)
        --y_worker_count;

    while (global_work_size[0] % x_worker_count)
        --x_worker_count;

    local_work_size[0] = x_worker_count; /* Multiple of image_width=1080 */
    local_work_size[1] = y_worker_count; /* Multiple of image_height=1280 */
    /* Launch the kernel over 1D grid */
    local_work_size[0] = x_worker_count; /* Multiple of image_width=1080 */
    local_work_size[1] = y_worker_count; /* Multiple of image_height=1280 */
    UFO_RESOURCES_CHECK_CLERR (clEnqueueNDRangeKernel (cmd_queue,
                                                       kernel,
                                                       2, NULL, global_work_size,
                                                       local_work_size,
                                                       0, NULL, NULL));
}

static void
get_background_image (UfoDenoiseTaskPrivate *priv, UfoBuffer *src, UfoBuffer *dst,
                      cl_command_queue cmd_queue)
{
    size_t dimension = priv->matrix_size;
    UfoRequisition image_requisition;
    ufo_buffer_get_requisition(src, &image_requisition);
    UfoRequisition requisition = {
        .n_dims = 3,
        .dims[0] = image_requisition.dims[0],
        .dims[1] = image_requisition.dims[1],
        .dims[2] = dimension * dimension,
    };
    UfoBuffer *ufo_buffer = ufo_buffer_new(&requisition, priv->context);

    /* loads surrounding dimension * dimension pixels of each pixel in image.
     * Result in buffer */
    launch_kernel_2D (priv, priv->k_load_elements, src, ufo_buffer, cmd_queue, dimension);

    /* Create background image using sorted neigbhooring pixels */
    launch_kernel_1D (priv->k_sort_and_set, ufo_buffer, dst, cmd_queue, dimension);

    g_object_unref(ufo_buffer);
}

static gboolean
ufo_denoise_task_process (UfoTask *task,
                          UfoBuffer **inputs,
                          UfoBuffer *output,
                          UfoRequisition *requisition)
{
    UfoDenoiseTaskPrivate *priv;
    UfoGpuNode *node;
    cl_command_queue cmd_queue;

    priv = UFO_DENOISE_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);

    get_background_image (priv, inputs[0], output, cmd_queue);
    launch_kernel_2D (priv, priv->k_remove_background, inputs[0], output, cmd_queue, 0);
    return TRUE;
}

static void
ufo_denoise_task_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    UfoDenoiseTaskPrivate *priv = UFO_DENOISE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_MATRIX_SIZE:
            priv->matrix_size = g_value_get_uint(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_denoise_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoDenoiseTaskPrivate *priv = UFO_DENOISE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_MATRIX_SIZE:
            g_value_set_uint (value, priv->matrix_size);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_denoise_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_denoise_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_denoise_task_setup;
    iface->get_num_inputs = ufo_denoise_task_get_num_inputs;
    iface->get_num_dimensions = ufo_denoise_task_get_num_dimensions;
    iface->get_mode = ufo_denoise_task_get_mode;
    iface->get_requisition = ufo_denoise_task_get_requisition;
    iface->process = ufo_denoise_task_process;
}

static void
ufo_denoise_task_class_init (UfoDenoiseTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_denoise_task_set_property;
    gobject_class->get_property = ufo_denoise_task_get_property;
    gobject_class->finalize = ufo_denoise_task_finalize;

    properties[PROP_MATRIX_SIZE] =
        g_param_spec_uint ("matrix-size",
            "determines the number of surrounding pixels to be compared with",
            "determines the number of surrounding pixels to be compared with",
            1, G_MAXUINT, 13,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoDenoiseTaskPrivate));
}

static void
ufo_denoise_task_init(UfoDenoiseTask *self)
{
    self->priv = UFO_DENOISE_TASK_GET_PRIVATE(self);
    self->priv->context = NULL;
    self->priv->matrix_size = 13;
    self->priv->k_sort_and_set = NULL;
    self->priv->k_load_elements = NULL;
    self->priv->k_remove_background = NULL;
}
