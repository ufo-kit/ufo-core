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
#include <string.h> 
#include <glib.h>
#include <glib/gprintf.h>

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "ufo-calculate-task.h"


struct _UfoCalculateTaskPrivate {
    cl_context context;
    cl_program program;
    cl_kernel kernel;
    gchar *expression;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoCalculateTask, ufo_calculate_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_CALCULATE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CALCULATE_TASK, UfoCalculateTaskPrivate))

enum {
    PROP_0,
    PROP_EXPRESSION,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void
make_kernel (UfoCalculateTaskPrivate *priv, UfoResources *resources, GError **error)
{
    const gchar *template = "kernel void calculate (global float *input, "\
                            "global float *output) {int x = get_global_id (0); "\
                            "float v = input[x]; output[x] = %s;}";
    gchar default_expression[] = "0.0f";
    gchar *source, *expression;
        
    expression = priv->expression == NULL ? default_expression : priv->expression;
    source = (gchar *) g_try_malloc (strlen (template) + strlen (expression));

    if (source == NULL) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "Could not allocate kernel string memory");
        return;
    }

    if (priv->kernel)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clReleaseKernel (priv->kernel), error);

    if ((gsize) g_sprintf (source, template, expression) != strlen (source)) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "Could not write kernel soruce");
        return;
    }

    priv->kernel = ufo_resources_get_kernel_from_source(resources, source, "calculate", NULL, error);
    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
    g_free (source);
}

UfoNode *
ufo_calculate_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_CALCULATE_TASK, NULL));
}

static void
ufo_calculate_task_setup (UfoTask *task,
                          UfoResources *resources,
                          GError **error)
{
    UfoCalculateTaskPrivate *priv;

    priv = UFO_CALCULATE_TASK_GET_PRIVATE (task);

    priv->context = ufo_resources_get_context (resources);
    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);
    make_kernel (priv, resources, error);
}

static void
ufo_calculate_task_get_requisition (UfoTask *task,
                                    UfoBuffer **inputs,
                                    UfoRequisition *requisition,
                                    GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_calculate_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_calculate_task_get_num_dimensions (UfoTask *task,
                                       guint input)
{
    g_return_val_if_fail (input == 0, 0);
    return 2;
}

static UfoTaskMode
ufo_calculate_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_calculate_task_process (UfoTask *task,
                            UfoBuffer **inputs,
                            UfoBuffer *output,
                            UfoRequisition *requisition)
{
    UfoCalculateTaskPrivate *priv;
    UfoRequisition in_req;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue *cmd_queue;
    cl_mem in_mem;
    cl_mem out_mem;
    gsize global_work_size = 1;
    guint i;

    priv = UFO_CALCULATE_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], &in_req);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE(task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &out_mem));

    for (i = 0; i < in_req.n_dims; i++) {
        global_work_size *= in_req.dims[i];
    }
    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 1, &global_work_size, NULL);

    return TRUE;
}

static void
ufo_calculate_task_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
    UfoCalculateTaskPrivate *priv = UFO_CALCULATE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_EXPRESSION:
            priv->expression = g_value_dup_string (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_calculate_task_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
    UfoCalculateTaskPrivate *priv = UFO_CALCULATE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_EXPRESSION:
            g_value_set_string (value, priv->expression ? priv->expression : "");
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_calculate_task_finalize (GObject *object)
{
    UfoCalculateTaskPrivate *priv = UFO_CALCULATE_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        clReleaseKernel (priv->kernel);
        priv->kernel = NULL;
    }
    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }

    g_free (priv->expression);

    G_OBJECT_CLASS (ufo_calculate_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_calculate_task_setup;
    iface->get_num_inputs = ufo_calculate_task_get_num_inputs;
    iface->get_num_dimensions = ufo_calculate_task_get_num_dimensions;
    iface->get_mode = ufo_calculate_task_get_mode;
    iface->get_requisition = ufo_calculate_task_get_requisition;
    iface->process = ufo_calculate_task_process;
}

static void
ufo_calculate_task_class_init (UfoCalculateTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_calculate_task_set_property;
    oclass->get_property = ufo_calculate_task_get_property;
    oclass->finalize = ufo_calculate_task_finalize;

    properties[PROP_EXPRESSION] =
        g_param_spec_string ("expression",
            "Arithmetic expression to calculate",
            "Arithmetic expression to calculate, you can use \"v\""
                "to access the values in the input and \"x\""
                "to access the indices of the input values",
            "0.0f",
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoCalculateTaskPrivate));
}

static void
ufo_calculate_task_init(UfoCalculateTask *self)
{
    self->priv = UFO_CALCULATE_TASK_GET_PRIVATE(self);
    self->priv->expression = NULL;
}
