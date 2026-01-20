/*
 * Copyright (C) 2011-2017 Karlsruhe Institute of Technology
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

#include "ufo-remove-outliers-task.h"


struct _UfoRemoveOutliersTaskPrivate {
    cl_kernel kernel;
    guint size;
    gfloat threshold;
    gint sign;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoRemoveOutliersTask, ufo_remove_outliers_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_REMOVE_OUTLIERS_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_REMOVE_OUTLIERS_TASK, UfoRemoveOutliersTaskPrivate))

enum {
    PROP_0,
    PROP_SIZE,
    PROP_THRESHOLD,
    PROP_SIGN,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_remove_outliers_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_REMOVE_OUTLIERS_TASK, NULL));
}

static void
ufo_remove_outliers_task_setup (UfoTask *task,
                       UfoResources *resources,
                       GError **error)
{
    UfoRemoveOutliersTaskPrivate *priv;
    gchar *option;

    priv = UFO_REMOVE_OUTLIERS_TASK_GET_PRIVATE (task);
    option = g_strdup_printf (" -DBOX_SIZE=%i ", priv->size);

    priv->kernel = ufo_resources_get_kernel (resources, "rm-outliers.cl", "filter", option, error);

    if (priv->kernel != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
}

static void
ufo_remove_outliers_task_get_requisition (UfoTask *task,
                                          UfoBuffer **inputs,
                                          UfoRequisition *requisition,
                                          GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_remove_outliers_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_remove_outliers_task_get_num_dimensions (UfoTask *task,
                                             guint input)
{
    return 2;
}

static UfoTaskMode
ufo_remove_outliers_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_remove_outliers_task_process (UfoTask *task,
                         UfoBuffer **inputs,
                         UfoBuffer *output,
                         UfoRequisition *requisition)
{
    UfoRemoveOutliersTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    cl_mem in_mem;
    cl_mem out_mem;

    priv = UFO_REMOVE_OUTLIERS_TASK_GET_PRIVATE (task);

    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (gfloat), &priv->threshold));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 3, sizeof (gint),  &priv->sign));

    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, requisition->dims, NULL);

    return TRUE;
}


static void
ufo_remove_outliers_task_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    UfoRemoveOutliersTaskPrivate *priv = UFO_REMOVE_OUTLIERS_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_SIZE:
            {
                guint new_size;

                new_size = g_value_get_uint (value);

                if ((new_size % 2) == 0)
                    g_warning ("RemoveOutliers::size = %i is divisible by 2, ignoring it", new_size);
                else
                    priv->size = new_size;
            }
            break;
        case PROP_THRESHOLD:
            priv->threshold = g_value_get_float (value);
            break;
        case PROP_SIGN:
            {
                gint new_sign;
                new_sign = g_value_get_int (value);
                if ((new_sign != 1) && (new_sign != -1))
                    g_warning ("RemoveOutliers::sign = %i is neither -1 nor 1, ignoring it", new_sign);
                else
                    priv->sign = new_sign;
             }
             break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_remove_outliers_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoRemoveOutliersTaskPrivate *priv = UFO_REMOVE_OUTLIERS_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_SIZE:
            g_value_set_uint (value, priv->size);
            break;
        case PROP_THRESHOLD:
            g_value_set_float (value, priv->threshold);
            break;
        case PROP_SIGN:
            g_value_set_int (value, priv->sign);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_remove_outliers_task_finalize (GObject *object)
{
    UfoRemoveOutliersTaskPrivate *priv = UFO_REMOVE_OUTLIERS_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }


    G_OBJECT_CLASS (ufo_remove_outliers_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_remove_outliers_task_setup;
    iface->get_num_inputs = ufo_remove_outliers_task_get_num_inputs;
    iface->get_num_dimensions = ufo_remove_outliers_task_get_num_dimensions;
    iface->get_mode = ufo_remove_outliers_task_get_mode;
    iface->get_requisition = ufo_remove_outliers_task_get_requisition;
    iface->process = ufo_remove_outliers_task_process;
}

static void
ufo_remove_outliers_task_class_init (UfoRemoveOutliersTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_remove_outliers_task_set_property;
    oclass->get_property = ufo_remove_outliers_task_get_property;
    oclass->finalize = ufo_remove_outliers_task_finalize;

    properties[PROP_SIZE] =
        g_param_spec_uint ("size",
            "Size of median box",
            "Size of median box",
            3, 33, 3,
            G_PARAM_READWRITE);

    properties[PROP_THRESHOLD] =
        g_param_spec_float ("threshold",
            "Threshold",
            "Minimum deviation from median for outlier",
            -G_MAXFLOAT, G_MAXFLOAT, 1.0f,
            G_PARAM_READWRITE);

    properties[PROP_SIGN] =
        g_param_spec_int ("sign",
            "Type of outliers",
            "1 for bright; -1 for dark",
            -1, 1, 1,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoRemoveOutliersTaskPrivate));
}

static void
ufo_remove_outliers_task_init(UfoRemoveOutliersTask *self)
{
    self->priv = UFO_REMOVE_OUTLIERS_TASK_GET_PRIVATE(self);
    self->priv->size = 3;
    self->priv->threshold = 1.0f;
    self->priv->sign = 1;
}
