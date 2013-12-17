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

#include <string.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <ufo/ufo-gpu-task-iface.h>
#include "ufo-opencl-task.h"

/**
 * SECTION:ufo-opencl-task
 * @Short_description: Process arbitrary OpenCL kernels
 * @Title: opencl
 *
 * This module is used to load an arbitrary #UfoOpenCLTask:kernel from
 * #UfoOpenCLTask:filename and execute it on each input. The kernel must have
 * only two global float array parameters, the first represents the input, the
 * second one the output. #UfoOpenCLTask:num-dims must be changed, if the kernel
 * accesses either one or three dimensional index spaces.
 */

struct _UfoOpenCLTaskPrivate {
    cl_kernel kernel;
    cl_uint n_inputs;
    gchar *filename;
    gchar *funcname;
    gchar *source;
    guint n_dims;
};

static void ufo_task_interface_init (UfoTaskIface *iface);
static void ufo_gpu_task_interface_init (UfoGpuTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoOpenCLTask, ufo_opencl_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init)
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_GPU_TASK,
                                                ufo_gpu_task_interface_init))

#define UFO_OPENCL_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_OPENCL_TASK, UfoOpenCLTaskPrivate))

enum {
    PROP_0,
    PROP_FILENAME,
    PROP_SOURCE,
    PROP_KERNEL,
    PROP_NUM_DIMS,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_opencl_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_OPENCL_TASK, NULL));
}

const gchar *
ufo_opencl_task_get_source (UfoOpenCLTask *task)
{
    g_return_val_if_fail (UFO_IS_OPENCL_TASK (task), NULL);

    /* TODO: return kernel source also from file */
    return task->priv->source;
}

static gboolean
ufo_opencl_task_process (UfoGpuTask *task,
                         UfoBuffer **inputs,
                         UfoBuffer *output,
                         UfoRequisition *requisition)
{
    UfoOpenCLTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    cl_mem out_mem;

    priv = UFO_OPENCL_TASK (task)->priv;
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);

    for (guint i = 0; i < priv->n_inputs; i++) {
        cl_mem in_mem;

        in_mem = ufo_buffer_get_device_array (inputs[i], cmd_queue);
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, i, sizeof (cl_mem), &in_mem));
    }

    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, priv->n_inputs, sizeof (cl_mem), &out_mem));

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, priv->kernel, priv->n_dims, requisition->dims, NULL);

    return TRUE;
}

static void
ufo_opencl_task_setup (UfoTask *task,
                       UfoResources *resources,
                       GError **error)
{
    UfoOpenCLTaskPrivate *priv;

    priv = UFO_OPENCL_TASK_GET_PRIVATE (task);

    if (priv->filename == NULL && priv->source == NULL) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "Neither property ::filename nor ::source specified");
        return;
    }

    if (priv->source != NULL) {
        priv->kernel = ufo_resources_get_kernel_from_source (resources,
                                                             priv->source,
                                                             priv->funcname,
                                                             error);
    }
    else {
        priv->kernel = ufo_resources_get_kernel (resources,
                                                 priv->filename,
                                                 priv->funcname,
                                                 error);
    }

    if (priv->kernel != NULL) {
        cl_uint n_args;

        UFO_RESOURCES_CHECK_CLERR (clGetKernelInfo (priv->kernel,
                                   CL_KERNEL_NUM_ARGS,
                                   sizeof (cl_uint),
                                   &n_args, NULL));

        if (n_args < 2) {
            g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                         "Kernel `%s' must accept at least two arguments",
                         priv->funcname);
            return;
        }

        priv->n_inputs = n_args - 1;
        UFO_RESOURCES_CHECK_CLERR (clRetainKernel (priv->kernel));
    }
}

static void
ufo_opencl_task_get_requisition (UfoTask *task,
                                 UfoBuffer **inputs,
                                 UfoRequisition *requisition)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static void
ufo_opencl_task_get_structure (UfoTask *task,
                               guint *n_inputs,
                               UfoInputParam **in_params,
                               UfoTaskMode *mode)
{
    UfoOpenCLTaskPrivate *priv;

    priv = UFO_OPENCL_TASK_GET_PRIVATE (task);
    *mode = UFO_TASK_MODE_PROCESSOR;
    *n_inputs = priv->n_inputs;
    *in_params = g_new0 (UfoInputParam, priv->n_inputs);

    for (guint i = 0; i < priv->n_inputs; i++) {
        (*in_params)[i].n_dims = priv->n_dims;
    }
}

static UfoNode *
ufo_opencl_task_copy_real (UfoNode *node,
                           GError **error)
{
    UfoOpenCLTask *orig;
    UfoOpenCLTask *copy;

    orig = UFO_OPENCL_TASK (node);
    copy = UFO_OPENCL_TASK (ufo_opencl_task_new ());
    copy->priv->n_inputs = orig->priv->n_inputs;

    g_object_set (G_OBJECT (copy),
                  "filename", orig->priv->filename,
                  "source", orig->priv->source,
                  "kernel", orig->priv->funcname,
                  "num-dims", orig->priv->n_dims,
                  NULL);

    return UFO_NODE (copy);
}

static gboolean
ufo_opencl_task_equal_real (UfoNode *n1,
                            UfoNode *n2)
{
    g_return_val_if_fail (UFO_IS_OPENCL_TASK (n1) && UFO_IS_OPENCL_TASK (n2), FALSE);
    return UFO_OPENCL_TASK (n1)->priv->kernel == UFO_OPENCL_TASK (n2)->priv->kernel;
}

static void
ufo_opencl_task_finalize (GObject *object)
{
    UfoOpenCLTaskPrivate *priv;

    priv = UFO_OPENCL_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        clReleaseKernel (priv->kernel);
        priv->kernel = NULL;
    }

    g_free (priv->filename);
    g_free (priv->funcname);

    priv->filename = NULL;
    priv->funcname = NULL;
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_opencl_task_setup;
    iface->get_requisition = ufo_opencl_task_get_requisition;
    iface->get_structure = ufo_opencl_task_get_structure;
}

static void
ufo_gpu_task_interface_init (UfoGpuTaskIface *iface)
{
    iface->process = ufo_opencl_task_process;
}

static void
copy_string_if_non_zero_length (const GValue *value,
                                gchar **str_location)
{
    const gchar *s;

    s = g_value_get_string (value);

    if (s && strlen (g_value_get_string (value)) > 0) {
        g_free (*str_location);
        *str_location = g_value_dup_string (value);
    }
}

static void
ufo_opencl_task_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    UfoOpenCLTaskPrivate *priv = UFO_OPENCL_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_FILENAME:
            copy_string_if_non_zero_length (value, &priv->filename);
            break;
        case PROP_SOURCE:
            copy_string_if_non_zero_length (value, &priv->source);
            break;
        case PROP_KERNEL:
            copy_string_if_non_zero_length (value, &priv->funcname);
            break;
        case PROP_NUM_DIMS:
            priv->n_dims = g_value_get_uint (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_opencl_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoOpenCLTaskPrivate *priv = UFO_OPENCL_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_FILENAME:
            g_value_set_string (value, priv->filename);
            break;
        case PROP_SOURCE:
            g_value_set_string (value, priv->source);
            break;
        case PROP_KERNEL:
            g_value_set_string (value, priv->funcname);
            break;
        case PROP_NUM_DIMS:
            g_value_set_uint (value, priv->n_dims);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_opencl_task_class_init (UfoOpenCLTaskClass *klass)
{
    GObjectClass *oclass;
    UfoNodeClass *node_class;

    oclass = G_OBJECT_CLASS (klass);
    node_class = UFO_NODE_CLASS (klass);

    oclass->finalize = ufo_opencl_task_finalize;
    oclass->set_property = ufo_opencl_task_set_property;
    oclass->get_property = ufo_opencl_task_get_property;

    properties[PROP_FILENAME] =
        g_param_spec_string ("filename",
            "OpenCL kernel filename",
            "OpenCL kernel filename",
            "",
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    properties[PROP_SOURCE] =
        g_param_spec_string ("source",
            "OpenCL kernel source",
            "OpenCL kernel source",
            "",
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    properties[PROP_KERNEL] =
        g_param_spec_string ("kernel",
            "Kernel name or NULL",
            "Name of the kernel that should be computed with this task or NULL",
            "",
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

    properties[PROP_NUM_DIMS] =
        g_param_spec_uint ("num-dims",
                           "Number of dimensions",
                           "Number of dimensions that the kernel works on",
                           1, 3, 2,
                           G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    node_class->copy = ufo_opencl_task_copy_real;
    node_class->equal = ufo_opencl_task_equal_real;

    g_type_class_add_private(klass, sizeof(UfoOpenCLTaskPrivate));
}

static void
ufo_opencl_task_init (UfoOpenCLTask *self)
{
    UfoOpenCLTaskPrivate *priv;
    self->priv = priv = UFO_OPENCL_TASK_GET_PRIVATE (self);
    priv->kernel = NULL;
    priv->filename = NULL;
    priv->funcname = NULL;
    priv->source = NULL;
    priv->n_dims = 2;
    priv->n_inputs = 1;
}
