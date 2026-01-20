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
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include <math.h>
#include "ufo-flat-field-correct-task.h"


struct _UfoFlatFieldCorrectTaskPrivate {
    gboolean fix_nan_and_inf;
    gboolean absorptivity;
    gboolean sinogram_input;
    gfloat dark_scale;
    gfloat flat_scale;
    cl_kernel kernel;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoFlatFieldCorrectTask, ufo_flat_field_correct_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_FLAT_FIELD_CORRECT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FLAT_FIELD_CORRECT_TASK, UfoFlatFieldCorrectTaskPrivate))

enum {
    PROP_0,
    PROP_FIX_NAN_AND_INF,
    PROP_ABSORPTIVITY,
    PROP_SINOGRAM_INPUT,
    PROP_DARK_SCALE,
    PROP_FLAT_SCALE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_flat_field_correct_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_FLAT_FIELD_CORRECT_TASK, NULL));
}

static void
ufo_flat_field_correct_task_setup (UfoTask *task,
                                   UfoResources *resources,
                                   GError **error)
{
    UfoFlatFieldCorrectTaskPrivate *priv;

    priv = UFO_FLAT_FIELD_CORRECT_TASK_GET_PRIVATE (task);
    priv->kernel = ufo_resources_get_kernel (resources, "ffc.cl", "flat_correct", NULL, error);

    if (priv->kernel)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
}

static void
ufo_flat_field_correct_task_get_requisition (UfoTask *task,
                                             UfoBuffer **inputs,
                                             UfoRequisition *requisition,
                                             GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);

    if (ufo_buffer_cmp_dimensions (inputs[1], requisition) != 0 ||
        ufo_buffer_cmp_dimensions (inputs[2], requisition) != 0) {
        g_set_error_literal (error, UFO_TASK_ERROR, UFO_TASK_ERROR_GET_REQUISITION,
                             "flat-field-correct inputs must have the same size");
    }
}

static guint
ufo_flat_field_correct_task_get_num_inputs (UfoTask *task)
{
    return 3;
}

static guint
ufo_flat_field_correct_task_get_num_dimensions (UfoTask *task, guint input)
{
    UfoFlatFieldCorrectTaskPrivate *priv;

    priv = UFO_FLAT_FIELD_CORRECT_TASK_GET_PRIVATE (task);
    g_return_val_if_fail (input <= 2, 0);

    /* A sinogram */
    if (input == 0)
        return 2;

    /* A row of dark frame depend on the sinogram_input flag */
    return priv->sinogram_input ? 1 : 2;
}

static UfoTaskMode
ufo_flat_field_correct_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_flat_field_correct_task_process (UfoTask *task,
                                        UfoBuffer **inputs,
                                        UfoBuffer *output,
                                        UfoRequisition *requisition)
{
    UfoFlatFieldCorrectTaskPrivate *priv;
    UfoProfiler *profiler;
    UfoGpuNode *node;

    cl_command_queue cmd_queue;
    cl_mem proj_mem;
    cl_mem dark_mem;
    cl_mem flat_mem;
    cl_mem out_mem;
    gint absorptivity, sino_in, fix_nan_and_inf;

    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    proj_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
    dark_mem = ufo_buffer_get_device_array (inputs[1], cmd_queue);
    flat_mem = ufo_buffer_get_device_array (inputs[2], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);

    priv = UFO_FLAT_FIELD_CORRECT_TASK_GET_PRIVATE (task);
    absorptivity = (gint) priv->absorptivity;
    sino_in = (gint) priv->sinogram_input;
    fix_nan_and_inf = (gint) priv->fix_nan_and_inf;

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &proj_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (cl_mem), &dark_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 3, sizeof (cl_mem), &flat_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 4, sizeof (cl_int), &sino_in));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 5, sizeof (cl_int), &absorptivity));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 6, sizeof (cl_int), &fix_nan_and_inf));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 7, sizeof (cl_float), &priv->dark_scale));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 8, sizeof (cl_float), &priv->flat_scale));

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, requisition->dims, NULL);

    return TRUE;
}

static void
ufo_flat_field_correct_task_set_property (GObject *object,
                                             guint property_id,
                                             const GValue *value,
                                             GParamSpec *pspec)
{
    UfoFlatFieldCorrectTaskPrivate *priv = UFO_FLAT_FIELD_CORRECT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_FIX_NAN_AND_INF:
            priv->fix_nan_and_inf = g_value_get_boolean (value);
            break;
        case PROP_ABSORPTIVITY:
            priv->absorptivity = g_value_get_boolean (value);
            break;
        case PROP_SINOGRAM_INPUT:
            priv->sinogram_input = g_value_get_boolean (value);
            break;
        case PROP_DARK_SCALE:
            priv->dark_scale = g_value_get_float (value);
            break;
        case PROP_FLAT_SCALE:
            priv->flat_scale = g_value_get_float (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_flat_field_correct_task_get_property (GObject *object,
                                             guint property_id,
                                             GValue *value,
                                             GParamSpec *pspec)
{
    UfoFlatFieldCorrectTaskPrivate *priv = UFO_FLAT_FIELD_CORRECT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_FIX_NAN_AND_INF:
            g_value_set_boolean (value, priv->fix_nan_and_inf);
            break;
        case PROP_ABSORPTIVITY:
            g_value_set_boolean (value, priv->absorptivity);
            break;
        case PROP_SINOGRAM_INPUT:
            g_value_set_boolean (value, priv->sinogram_input);
            break;
        case PROP_DARK_SCALE:
            g_value_set_float (value, priv->dark_scale);
            break;
        case PROP_FLAT_SCALE:
            g_value_set_float (value, priv->flat_scale);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_flat_field_correct_task_finalize (GObject *object)
{
    UfoFlatFieldCorrectTaskPrivate *priv;

    priv = UFO_FLAT_FIELD_CORRECT_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }

    G_OBJECT_CLASS (ufo_flat_field_correct_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_flat_field_correct_task_setup;
    iface->get_num_inputs = ufo_flat_field_correct_task_get_num_inputs;
    iface->get_num_dimensions = ufo_flat_field_correct_task_get_num_dimensions;
    iface->get_mode = ufo_flat_field_correct_task_get_mode;
    iface->get_requisition = ufo_flat_field_correct_task_get_requisition;
    iface->process = ufo_flat_field_correct_task_process;
}

static void
ufo_flat_field_correct_task_class_init (UfoFlatFieldCorrectTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_flat_field_correct_task_set_property;
    gobject_class->get_property = ufo_flat_field_correct_task_get_property;
    gobject_class->finalize = ufo_flat_field_correct_task_finalize;

    properties[PROP_FIX_NAN_AND_INF] =
        g_param_spec_boolean("fix-nan-and-inf",
            "Replace NAN and INF values with 0.0",
            "Replace NAN and INF values with 0.0",
            FALSE,
            G_PARAM_READWRITE);

    properties[PROP_ABSORPTIVITY] =
        g_param_spec_boolean ("absorption-correct",
            "Absorption correct",
            "Absorption correct",
            FALSE,
            G_PARAM_READWRITE);

    properties[PROP_SINOGRAM_INPUT] =
        g_param_spec_boolean ("sinogram-input",
            "If sinogram-input is True we correct only one line (the sinogram), thus darks are flats are 1D",
            "If sinogram-input is True we correct only one line (the sinogram), thus darks are flats are 1D",
            FALSE,
            G_PARAM_READWRITE);

    properties[PROP_DARK_SCALE] =
        g_param_spec_float ("dark-scale",
            "Scale the dark field prior to the flat field correct",
            "Scale the dark field prior to the flat field correct",
            -G_MAXFLOAT, G_MAXFLOAT, 1.0f,
            G_PARAM_READWRITE);

    properties[PROP_FLAT_SCALE] =
        g_param_spec_float ("flat-scale",
            "Scale the flat field prior to the flat field correct",
            "Scale the flat field prior to the flat field correct",
            -G_MAXFLOAT, G_MAXFLOAT, 1.0f,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoFlatFieldCorrectTaskPrivate));
}

static void
ufo_flat_field_correct_task_init(UfoFlatFieldCorrectTask *self)
{
    self->priv = UFO_FLAT_FIELD_CORRECT_TASK_GET_PRIVATE(self);
    self->priv->fix_nan_and_inf = FALSE;
    self->priv->absorptivity = FALSE;
    self->priv->sinogram_input = FALSE;
    self->priv->kernel = NULL;
    self->priv->dark_scale = 1.0f;
    self->priv->flat_scale = 1.0f;
}
