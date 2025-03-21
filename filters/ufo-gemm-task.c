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

#include <clblast_c.h>

#include "ufo-gemm-task.h"


struct _UfoGemmTaskPrivate {
    gfloat alpha;
    gfloat beta;
    gsize m;
    gsize k;
    gsize n;
    gboolean error;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoGemmTask, ufo_gemm_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_GEMM_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GEMM_TASK, UfoGemmTaskPrivate))

enum {
    PROP_0,
    PROP_ALPHA,
    PROP_BETA,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_gemm_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_GEMM_TASK, NULL));
}

static void
ufo_gemm_task_setup (UfoTask *task,
                     UfoResources *resources,
                     GError **error)
{
}

static void
ufo_gemm_task_get_requisition (UfoTask *task,
                               UfoBuffer **inputs,
                               UfoRequisition *requisition,
                               GError **error)
{
    UfoGemmTaskPrivate *priv;
    UfoRequisition r_A;
    UfoRequisition r_B;
    UfoRequisition r_C;
    
    priv = UFO_GEMM_TASK_GET_PRIVATE (task);
    priv->error = FALSE;

    ufo_buffer_get_requisition (inputs[0], &r_A);
    ufo_buffer_get_requisition (inputs[1], &r_B);
    ufo_buffer_get_requisition (inputs[2], &r_C);

    if (r_B.dims[0] != r_A.dims[1]) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_GET_REQUISITION,
                     "A = <%zu, %zu> not compatible with B = <%zu, %zu>",
                     r_A.dims[0], r_A.dims[1], r_B.dims[0], r_B.dims[0]);

        priv->error = TRUE;
    }

    if ((r_C.dims[0] != r_A.dims[0]) || (r_C.dims[1] != r_B.dims[1])) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_GET_REQUISITION,
                     "C = <%zu, %zu> not compatible with A = <%zu, %zu> and B = <%zu, %zu>",
                     r_C.dims[0], r_C.dims[1], r_A.dims[0], r_A.dims[1], r_B.dims[0], r_B.dims[1]);

        priv->error = TRUE;
    }

    priv->m = r_A.dims[0];
    priv->k = r_A.dims[1];
    priv->n = r_B.dims[1];

    requisition->n_dims = 2;
    requisition->dims[0] = priv->m;
    requisition->dims[1] = priv->n;
}

static guint
ufo_gemm_task_get_num_inputs (UfoTask *task)
{
    return 3;
}

static guint
ufo_gemm_task_get_num_dimensions (UfoTask *task,
                                  guint input)
{
    return 2;
}

static UfoTaskMode
ufo_gemm_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_gemm_task_process (UfoTask *task,
                       UfoBuffer **inputs,
                       UfoBuffer *output,
                       UfoRequisition *requisition)
{
    UfoGemmTaskPrivate *priv;
    UfoGpuNode *node;
    cl_command_queue cmd_queue;
    cl_mem a_mem;
    cl_mem b_mem;
    cl_mem c_mem;
    cl_event event;
    CLBlastStatusCode code;
    
    priv = UFO_GEMM_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);

    a_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
    b_mem = ufo_buffer_get_device_array (inputs[1], cmd_queue);
    c_mem = ufo_buffer_get_device_array (inputs[2], cmd_queue);

    if (priv->error)
        return FALSE;

    code = CLBlastSgemm (CLBlastLayoutRowMajor, CLBlastTransposeNo, CLBlastTransposeNo,
                         priv->m, priv->n, priv->k,
                         priv->alpha,
                         a_mem, 0, priv->m,
                         b_mem, 0, priv->k,
                         priv->beta,
                         c_mem, 0, priv->m,
                         &cmd_queue, &event);

    if (code > CLBlastNotImplemented)
        UFO_RESOURCES_CHECK_CLERR (code);

    if (code == CLBlastSuccess) {
        cl_mem out_mem;
        cl_event copy_event;

        out_mem = ufo_buffer_get_device_array (output, cmd_queue);
        UFO_RESOURCES_CHECK_CLERR (clEnqueueCopyBuffer (cmd_queue, c_mem, out_mem, 0, 0, ufo_buffer_get_size (output), 1, &event, &copy_event));

        UFO_RESOURCES_CHECK_CLERR (clWaitForEvents (1, &copy_event));
        UFO_RESOURCES_CHECK_CLERR (clReleaseEvent (event));
        UFO_RESOURCES_CHECK_CLERR (clReleaseEvent (copy_event));
    }

    return TRUE;
}

static void
ufo_gemm_task_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    UfoGemmTaskPrivate *priv = UFO_GEMM_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_ALPHA:
            priv->alpha = g_value_get_float (value);
            break;
        case PROP_BETA:
            priv->beta = g_value_get_float (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_gemm_task_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    UfoGemmTaskPrivate *priv = UFO_GEMM_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_ALPHA:
            g_value_set_float (value, priv->alpha);
            break;
        case PROP_BETA:
            g_value_set_float (value, priv->beta);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_gemm_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_gemm_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_gemm_task_setup;
    iface->get_num_inputs = ufo_gemm_task_get_num_inputs;
    iface->get_num_dimensions = ufo_gemm_task_get_num_dimensions;
    iface->get_mode = ufo_gemm_task_get_mode;
    iface->get_requisition = ufo_gemm_task_get_requisition;
    iface->process = ufo_gemm_task_process;
}

static void
ufo_gemm_task_class_init (UfoGemmTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_gemm_task_set_property;
    oclass->get_property = ufo_gemm_task_get_property;
    oclass->finalize = ufo_gemm_task_finalize;

    properties[PROP_ALPHA] =
        g_param_spec_float ("alpha",
            "Scalar GEMM alpha value",
            "Scalar GEMM alpha value",
            -G_MAXFLOAT, G_MAXFLOAT, 1.0f,
            G_PARAM_READWRITE);

    properties[PROP_BETA] =
        g_param_spec_float ("beta",
            "Scalar GEMM beta value",
            "Scalar GEMM beta value",
            -G_MAXFLOAT, G_MAXFLOAT, 0.0f,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoGemmTaskPrivate));
}

static void
ufo_gemm_task_init(UfoGemmTask *self)
{
    self->priv = UFO_GEMM_TASK_GET_PRIVATE(self);
    self->priv->alpha = 1.0f;
    self->priv->beta = 0.0f;
}
