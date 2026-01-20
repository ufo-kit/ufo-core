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
# include <OpenCL/cl.h>
#else
# include <CL/cl.h>
#endif

#include <math.h>
#include "ufo-fftmult-task.h"
#include "ufo-priv.h"

struct _UfoFftmultTaskPrivate {
    cl_kernel k_fftmult;
    UfoResources *resources;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoFftmultTask, ufo_fftmult_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_FFTMULT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FFTMULT_TASK, UfoFftmultTaskPrivate))

UfoNode *
ufo_fftmult_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_FFTMULT_TASK, NULL));
}

static void
ufo_fftmult_task_setup (UfoTask *task,
                       UfoResources *resources,
                       GError **error)
{
    UfoFftmultTaskPrivate *priv;

    priv = UFO_FFTMULT_TASK_GET_PRIVATE (task);
    priv->resources = resources;

    priv->k_fftmult = ufo_resources_get_kernel (resources, "fftmult.cl", "mult", NULL, error);

    if (priv->k_fftmult != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->k_fftmult), error);
}

static void
ufo_fftmult_task_get_requisition (UfoTask *task,
                                  UfoBuffer **inputs,
                                  UfoRequisition *requisition,
                                  GError **error)
{
    ufo_buffer_get_requisition(inputs[1], requisition);
}

static guint
ufo_fftmult_task_get_num_inputs (UfoTask *task)
{
    return 2;
}

static guint
ufo_fftmult_task_get_num_dimensions (UfoTask *task,
                                     guint input)
{
    return 2;
}

static UfoTaskMode
ufo_fftmult_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static void
get_ring_metadata(UfoBuffer *src, unsigned *number_ones, unsigned *radius)
{
    GValue *value;
    value = ufo_buffer_get_metadata(src, "radius");
    *radius = g_value_get_uint(value);
    value = ufo_buffer_get_metadata(src, "number_ones");
    *number_ones = g_value_get_uint(value);
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
launch_kernel_2D(UfoFftmultTaskPrivate *priv,
                 UfoBuffer *ufo_a, UfoBuffer *ufo_b,
                 UfoBuffer *ufo_dst, cl_command_queue cmd_queue)
{
    cl_kernel kernel = priv->k_fftmult;
    cl_mem a, b, dst;
    UfoRequisition requisition;
    size_t global_work_size[2];
    size_t local_work_size[2];
    dst = ufo_buffer_get_device_array(ufo_dst, cmd_queue);
    a = ufo_buffer_get_device_array(ufo_a, cmd_queue);
    b = ufo_buffer_get_device_array(ufo_b, cmd_queue);

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 0, sizeof (cl_mem), &a));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 1, sizeof (cl_mem), &b));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 2, sizeof (cl_mem), &dst));

    /* Launch the kernel over 2D grid, using dst requisition which reperesents a
     * crop of the image */
    ufo_buffer_get_requisition (ufo_dst, &requisition);
    global_work_size[0] = requisition.dims[0] / 2;
    /* Buffer may have mod extra rows, don't take them into account */
    g_assert(requisition.dims[0] % 2 == 0 && "FFT images are multiples of 2\n");
    global_work_size[1] = requisition.dims[1];
    size_t y_worker_count, x_worker_count;
    get_max_work_group_size(priv->resources, &x_worker_count, &y_worker_count);

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

static gboolean
ufo_fftmult_task_process (UfoTask *task,
                          UfoBuffer **inputs,
                          UfoBuffer *output,
                          UfoRequisition *requisition)
{
    UfoFftmultTaskPrivate *priv;

    /* Forwarding ring radius metada to next plugin */
    unsigned radius, number_ones;
    get_ring_metadata(inputs[1], &number_ones, &radius);

    UfoGpuNode *node;
    cl_command_queue cmd_queue;

    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);

    priv = UFO_FFTMULT_TASK_GET_PRIVATE (task);
    launch_kernel_2D (priv, inputs[0], inputs[1], output, cmd_queue);
    return TRUE;
}

static void
ufo_fftmult_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_fftmult_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_fftmult_task_setup;
    iface->get_num_inputs = ufo_fftmult_task_get_num_inputs;
    iface->get_num_dimensions = ufo_fftmult_task_get_num_dimensions;
    iface->get_mode = ufo_fftmult_task_get_mode;
    iface->get_requisition = ufo_fftmult_task_get_requisition;
    iface->process = ufo_fftmult_task_process;
}

static void
ufo_fftmult_task_class_init (UfoFftmultTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = ufo_fftmult_task_finalize;

    g_type_class_add_private (oclass, sizeof(UfoFftmultTaskPrivate));
}

static void
ufo_fftmult_task_init(UfoFftmultTask *self)
{
    self->priv = UFO_FFTMULT_TASK_GET_PRIVATE(self);
}
