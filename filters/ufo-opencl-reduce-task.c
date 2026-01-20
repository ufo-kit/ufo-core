/*
 * Copyright (C) 2017 Karlsruhe Institute of Technology
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

#include "ufo-opencl-reduce-task.h"


struct _UfoOpenCLReduceTaskPrivate {
    cl_kernel kernel;
    cl_kernel finish;
    cl_uint n_inputs;
    gchar *filename;
    gchar *kernel_name;
    gchar *finish_name;
    gchar *source;
    guint n_dims;
    gboolean generated;
    gboolean fold;
    gfloat fold_value;
    cl_uint counter;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoOpenCLReduceTask, ufo_opencl_reduce_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_OPENCL_REDUCE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_OPENCL_REDUCE_TASK, UfoOpenCLReduceTaskPrivate))

enum {
    PROP_0,
    PROP_FILENAME,
    PROP_SOURCE,
    PROP_KERNEL,
    PROP_FINISH,
    PROP_NUM_DIMS,
    PROP_FOLD_VALUE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_opencl_reduce_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_OPENCL_REDUCE_TASK, NULL));
}

static void
ufo_opencl_reduce_task_setup (UfoTask *task,
                              UfoResources *resources,
                              GError **error)
{
    UfoOpenCLReduceTaskPrivate *priv;

    priv = UFO_OPENCL_REDUCE_TASK_GET_PRIVATE (task);

    if (priv->kernel_name == NULL) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "Must specify a ::kernel name to use for operation");
        return;

    }

    if (priv->filename != NULL && priv->source != NULL) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "Cannot use ::filename and ::source at the same time");
        return;
    }

    if (priv->source != NULL) {
        priv->kernel = ufo_resources_get_kernel_from_source (resources, priv->source, priv->kernel_name, NULL, error);

        if (priv->finish_name)
            priv->finish = ufo_resources_get_kernel_from_source (resources, priv->source, priv->finish_name, NULL, error);
    }
    else {
        const gchar *filename;

        filename = priv->filename != NULL ? priv->filename : "opencl-reduce.cl";
        priv->kernel = ufo_resources_get_kernel (resources, filename, priv->kernel_name, NULL, error);

        if (priv->finish_name)
            priv->finish = ufo_resources_get_kernel (resources, filename, priv->finish_name, NULL, error);
    }

    if (priv->kernel != NULL) {
        cl_uint n_args;

        UFO_RESOURCES_CHECK_SET_AND_RETURN (clGetKernelInfo (priv->kernel,
                                                             CL_KERNEL_NUM_ARGS,
                                                             sizeof (cl_uint),
                                                             &n_args, NULL),
                                            error);

        if (n_args != 2) {
            g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                         "Kernel `%s' must accept exactly two arguments",
                         priv->kernel_name);
            return;
        }

        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
    }

    if (priv->finish)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->finish), error);

    priv->generated = FALSE;
    priv->counter = 0;
}

static void
ufo_opencl_reduce_task_get_requisition (UfoTask *task,
                                        UfoBuffer **inputs,
                                        UfoRequisition *requisition,
                                        GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_opencl_reduce_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_opencl_reduce_task_get_num_dimensions (UfoTask *task,
                                           guint input)
{
    return UFO_OPENCL_REDUCE_TASK_GET_PRIVATE (task)->n_dims;
}

static UfoTaskMode
ufo_opencl_reduce_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_REDUCTOR | UFO_TASK_MODE_GPU;
}

static UfoNode *
ufo_opencl_reduce_task_copy_real (UfoNode *node,
                                  GError **error)
{
    g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_COPY,
                 "Cannot be copied (please disable graph expansion or limit the used devices to 1, e.g. UFO_DEVICES=0)");

    return NULL;
}

static gboolean
ufo_opencl_reduce_task_equal_real (UfoNode *n1,
                                   UfoNode *n2)
{
    g_return_val_if_fail (UFO_IS_OPENCL_REDUCE_TASK (n1) && UFO_IS_OPENCL_REDUCE_TASK (n2), FALSE);
    return UFO_OPENCL_REDUCE_TASK (n1)->priv->kernel == UFO_OPENCL_REDUCE_TASK (n2)->priv->kernel;
}

static gboolean
ufo_opencl_reduce_task_process (UfoTask *task,
                                UfoBuffer **inputs,
                                UfoBuffer *output,
                                UfoRequisition *requisition)
{
    UfoOpenCLReduceTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    cl_mem out_mem;
    cl_mem in_mem;

    priv = UFO_OPENCL_REDUCE_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);

    if (priv->counter == 0) {
        if (priv->fold) {
            UFO_RESOURCES_CHECK_CLERR (clEnqueueFillBuffer (cmd_queue, out_mem,
                                                            &priv->fold_value, sizeof (gfloat),
                                                            0, ufo_buffer_get_size (output),
                                                            0, NULL, NULL));
        }
        else {
            UFO_RESOURCES_CHECK_CLERR (clEnqueueCopyBuffer (cmd_queue, in_mem, out_mem,
                                                            0, 0, ufo_buffer_get_size (output),
                                                            0, NULL, NULL));
        }
    }

    /* 
     * We have to skip the first iteration for reduce operations because we
     * already copied the data into output buffer.
     */
    if (priv->fold || priv->counter > 0) {
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &in_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &out_mem));
        profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
        ufo_profiler_call (profiler, cmd_queue, priv->kernel, priv->n_dims, requisition->dims, NULL);
    }

    priv->counter++;
    return TRUE;
}

static gboolean
ufo_opencl_reduce_task_generate (UfoTask *task,
                                 UfoBuffer *output,
                                 UfoRequisition *requisition)
{
    UfoOpenCLReduceTaskPrivate *priv;

    priv = UFO_OPENCL_REDUCE_TASK_GET_PRIVATE (task);

    if (priv->generated)
        return FALSE;

    if (priv->finish) {
        UfoGpuNode *node;
        UfoProfiler *profiler;
        cl_command_queue cmd_queue;
        cl_mem out_mem;

        priv = UFO_OPENCL_REDUCE_TASK_GET_PRIVATE (task);
        node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
        cmd_queue = ufo_gpu_node_get_cmd_queue (node);
        out_mem = ufo_buffer_get_device_array (output, cmd_queue);

        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->finish, 0, sizeof (cl_mem), &out_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->finish, 1, sizeof (cl_uint), &priv->counter));
        profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
        ufo_profiler_call (profiler, cmd_queue, priv->finish, priv->n_dims, requisition->dims, NULL);
    }

    priv->generated = TRUE;
    return TRUE;
}

static void
ufo_opencl_reduce_task_set_property (GObject *object,
                                     guint property_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
    UfoOpenCLReduceTaskPrivate *priv = UFO_OPENCL_REDUCE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_FILENAME:
            g_free (priv->filename);
            priv->filename = g_value_dup_string (value);
            break;
        case PROP_SOURCE:
            g_free (priv->source);
            priv->source = g_value_dup_string (value);
            break;
        case PROP_KERNEL:
            g_free (priv->kernel_name);
            priv->kernel_name = g_value_dup_string (value);
            break;
        case PROP_FINISH:
            g_free (priv->finish_name);
            priv->finish_name = g_value_dup_string (value);
            break;
        case PROP_NUM_DIMS:
            priv->n_dims = g_value_get_uint (value);
            break;
        case PROP_FOLD_VALUE:
            priv->fold = TRUE;
            priv->fold_value = g_value_get_float (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_opencl_reduce_task_get_property (GObject *object,
                                     guint property_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
    UfoOpenCLReduceTaskPrivate *priv = UFO_OPENCL_REDUCE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_FILENAME:
            g_value_set_string (value, priv->filename ? priv->filename : "");
            break;
        case PROP_SOURCE:
            g_value_set_string (value, priv->source ? priv->source : "");
            break;
        case PROP_KERNEL:
            g_value_set_string (value, priv->kernel_name ? priv->kernel_name : "");
            break;
        case PROP_FINISH:
            g_value_set_string (value, priv->finish_name ? priv->finish_name : "");
            break;
        case PROP_NUM_DIMS:
            g_value_set_uint (value, priv->n_dims);
            break;
        case PROP_FOLD_VALUE:
            g_value_set_float (value, priv->fold_value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_opencl_reduce_task_finalize (GObject *object)
{
    UfoOpenCLReduceTaskPrivate *priv;

    priv = UFO_OPENCL_REDUCE_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        clReleaseKernel (priv->kernel);
        priv->kernel = NULL;
    }

    if (priv->finish) {
        clReleaseKernel (priv->finish);
        priv->finish = NULL;
    }

    g_free (priv->filename);
    g_free (priv->kernel_name);
    g_free (priv->finish_name);

    priv->filename = NULL;
    priv->kernel_name = NULL;
    priv->finish_name = NULL;
    G_OBJECT_CLASS (ufo_opencl_reduce_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_opencl_reduce_task_setup;
    iface->get_num_inputs = ufo_opencl_reduce_task_get_num_inputs;
    iface->get_num_dimensions = ufo_opencl_reduce_task_get_num_dimensions;
    iface->get_mode = ufo_opencl_reduce_task_get_mode;
    iface->get_requisition = ufo_opencl_reduce_task_get_requisition;
    iface->process = ufo_opencl_reduce_task_process;
    iface->generate = ufo_opencl_reduce_task_generate;
}

static void
ufo_opencl_reduce_task_class_init (UfoOpenCLReduceTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);
    UfoNodeClass *node_class = UFO_NODE_CLASS (klass);

    oclass->set_property = ufo_opencl_reduce_task_set_property;
    oclass->get_property = ufo_opencl_reduce_task_get_property;
    oclass->finalize = ufo_opencl_reduce_task_finalize;

    properties[PROP_FILENAME] =
        g_param_spec_string ("filename",
            "OpenCL kernel filename",
            "OpenCL kernel filename",
            "",
            G_PARAM_READWRITE);

    properties[PROP_SOURCE] =
        g_param_spec_string ("source",
            "OpenCL kernel source",
            "OpenCL kernel source",
            "",
            G_PARAM_READWRITE);

    properties[PROP_KERNEL] =
        g_param_spec_string ("kernel",
            "Kernel name",
            "Name of the kernel that should be computed with this task",
            "",
            G_PARAM_READWRITE);

    properties[PROP_FINISH] =
        g_param_spec_string ("finish",
            "Optional finish kernel name",
            "Name of the kernel that should be run at the end",
            "",
            G_PARAM_READWRITE);

    properties[PROP_FOLD_VALUE] =
        g_param_spec_float ("fold-value",
            "Initial fold value",
            "Initial fold value, if not set the operation is a real reduction",
            -G_MAXFLOAT, G_MAXFLOAT, 0.0f,
            G_PARAM_READWRITE);

    properties[PROP_NUM_DIMS] = 
        g_param_spec_uint ("dimensions",
            "Number of dimensions",
            "Number of dimensions that the kernel works on",
            1, 3, 2,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    node_class->copy = ufo_opencl_reduce_task_copy_real;
    node_class->equal = ufo_opencl_reduce_task_equal_real;

    g_type_class_add_private (oclass, sizeof(UfoOpenCLReduceTaskPrivate));
}

static void
ufo_opencl_reduce_task_init(UfoOpenCLReduceTask *self)
{
    self->priv = UFO_OPENCL_REDUCE_TASK_GET_PRIVATE(self);
    self->priv->kernel = NULL;
    self->priv->finish = NULL;
    self->priv->filename = NULL;
    self->priv->kernel_name = NULL;
    self->priv->finish_name = NULL;
    self->priv->source = NULL;
    self->priv->n_dims = 2;
    self->priv->fold = FALSE;
    self->priv->counter = 0;
}
