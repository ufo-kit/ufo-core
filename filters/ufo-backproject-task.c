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
#include "ufo-backproject-task.h"


typedef enum {
    MODE_NEAREST,
    MODE_TEXTURE
} Mode;

static GEnumValue mode_values[] = {
    { MODE_NEAREST, "MODE_NEAREST", "nearest" },
    { MODE_TEXTURE, "MODE_TEXTURE", "texture" },
    { 0, NULL, NULL}
};

struct _UfoBackprojectTaskPrivate {
    cl_context context;
    cl_kernel nearest_kernel;
    cl_kernel texture_kernel;
    cl_mem sin_lut;
    cl_mem cos_lut;
    gfloat *host_sin_lut;
    gfloat *host_cos_lut;
    gdouble axis_pos;
    gdouble angle_step;
    gdouble angle_offset;
    gdouble real_angle_step;
    gboolean luts_changed;
    guint offset;
    guint burst_projections;
    guint n_projections;
    guint roi_x;
    guint roi_y;
    gint roi_width;
    gint roi_height;
    Mode mode;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoBackprojectTask, ufo_backproject_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_BACKPROJECT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BACKPROJECT_TASK, UfoBackprojectTaskPrivate))

enum {
    PROP_0,
    PROP_NUM_PROJECTIONS,
    PROP_OFFSET,
    PROP_AXIS_POSITION,
    PROP_ANGLE_STEP,
    PROP_ANGLE_OFFSET,
    PROP_ROI_X,
    PROP_ROI_Y,
    PROP_ROI_WIDTH,
    PROP_ROI_HEIGHT,
    PROP_MODE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_backproject_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_BACKPROJECT_TASK, NULL));
}

static gboolean
ufo_backproject_task_process (UfoTask *task,
                              UfoBuffer **inputs,
                              UfoBuffer *output,
                              UfoRequisition *requisition)
{
    UfoBackprojectTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    cl_mem in_mem;
    cl_mem out_mem;
    cl_kernel kernel;
    gfloat axis_pos;

    priv = UFO_BACKPROJECT_TASK (task)->priv;
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);

    if (priv->mode == MODE_TEXTURE) {
        in_mem = ufo_buffer_get_device_image (inputs[0], cmd_queue);
        kernel = priv->texture_kernel;
    }
    else {
        in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
        kernel = priv->nearest_kernel;
    }

    /* Guess axis position if they are not provided by the user. */
    if (priv->axis_pos <= 0.0) {
        UfoRequisition in_req;

        ufo_buffer_get_requisition (inputs[0], &in_req);
        axis_pos = (gfloat) ((gfloat) in_req.dims[0]) / 2.0f;
    }
    else {
        axis_pos = priv->axis_pos;
    }

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 0, sizeof (cl_mem), &in_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 1, sizeof (cl_mem), &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 2, sizeof (cl_mem), &priv->sin_lut));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 3, sizeof (cl_mem), &priv->cos_lut));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 4, sizeof (guint),  &priv->roi_x));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 5, sizeof (guint),  &priv->roi_y));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 6, sizeof (guint),  &priv->offset));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 7, sizeof (guint),  &priv->burst_projections));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 8, sizeof (gfloat), &axis_pos));

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, kernel, 2, requisition->dims, NULL);

    return TRUE;
}

static void
ufo_backproject_task_setup (UfoTask *task,
                            UfoResources *resources,
                            GError **error)
{
    UfoBackprojectTaskPrivate *priv;

    priv = UFO_BACKPROJECT_TASK_GET_PRIVATE (task);

    priv->context = ufo_resources_get_context (resources);
    priv->nearest_kernel = ufo_resources_get_kernel (resources, "backproject.cl", "backproject_nearest", NULL, error);
    priv->texture_kernel = ufo_resources_get_kernel (resources, "backproject.cl", "backproject_tex", NULL, error);

    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);

    if (priv->nearest_kernel != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->nearest_kernel), error);

    if (priv->texture_kernel != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->texture_kernel), error);
}

static cl_mem
create_lut_buffer (UfoBackprojectTaskPrivate *priv,
                   gfloat **host_mem,
                   gsize n_entries,
                   double (*func)(double))
{
    cl_int errcode;
    gsize size = n_entries * sizeof (gfloat);
    cl_mem mem = NULL;

    *host_mem = g_realloc (*host_mem, size);

    for (guint i = 0; i < n_entries; i++)
        (*host_mem)[i] = (gfloat) func (priv->angle_offset + i * priv->real_angle_step);

    mem = clCreateBuffer (priv->context,
                          CL_MEM_COPY_HOST_PTR | CL_MEM_READ_ONLY,
                          size, *host_mem,
                          &errcode);

    UFO_RESOURCES_CHECK_CLERR (errcode);
    return mem;
}

static void
release_lut_mems (UfoBackprojectTaskPrivate *priv)
{
    if (priv->sin_lut) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->sin_lut));
        priv->sin_lut = NULL;
    }

    if (priv->cos_lut) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->cos_lut));
        priv->cos_lut = NULL;
    }
}

static void
ufo_backproject_task_get_requisition (UfoTask *task,
                                      UfoBuffer **inputs,
                                      UfoRequisition *requisition,
                                      GError **error)
{
    UfoBackprojectTaskPrivate *priv;
    UfoRequisition in_req;

    priv = UFO_BACKPROJECT_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], &in_req);

    /* If the number of projections is not specified use the input size */
    if (priv->n_projections == 0) {
        priv->n_projections = (guint) in_req.dims[1];
    }

    priv->burst_projections = (guint) in_req.dims[1];

    if (priv->burst_projections > priv->n_projections) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_GET_REQUISITION,
                     "Total number of projections (%u) must be greater than "
                     "or equal to sinogram height (%u)",
                     priv->n_projections, priv->burst_projections);
        return;
    }

    requisition->n_dims = 2;

    /* TODO: we should check here, that we might access data outside the
     * projections */
    requisition->dims[0] = priv->roi_width == 0 ? in_req.dims[0] : (gsize) priv->roi_width;
    requisition->dims[1] = priv->roi_height == 0 ? in_req.dims[0] : (gsize) priv->roi_height;

    if (priv->real_angle_step < 0.0) {
        if (priv->angle_step <= 0.0)
            priv->real_angle_step = G_PI / ((gdouble) priv->n_projections);
        else
            priv->real_angle_step = priv->angle_step;
    }

    if (priv->luts_changed) {
        release_lut_mems (priv);
        priv->luts_changed = FALSE;
    }

    if (priv->sin_lut == NULL) {
        priv->sin_lut = create_lut_buffer (priv, &priv->host_sin_lut,
                                           priv->n_projections, sin);
    }

    if (priv->cos_lut == NULL) {
        priv->cos_lut = create_lut_buffer (priv, &priv->host_cos_lut,
                                           priv->n_projections, cos);
    }
}

static guint
ufo_filter_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_filter_task_get_num_dimensions (UfoTask *task,
                               guint input)
{
    g_return_val_if_fail (input == 0, 0);
    return 2;
}

static UfoTaskMode
ufo_filter_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_backproject_task_equal_real (UfoNode *n1,
                            UfoNode *n2)
{
    g_return_val_if_fail (UFO_IS_BACKPROJECT_TASK (n1) && UFO_IS_BACKPROJECT_TASK (n2), FALSE);
    return UFO_BACKPROJECT_TASK (n1)->priv->texture_kernel == UFO_BACKPROJECT_TASK (n2)->priv->texture_kernel;
}

static void
ufo_backproject_task_finalize (GObject *object)
{
    UfoBackprojectTaskPrivate *priv;

    priv = UFO_BACKPROJECT_TASK_GET_PRIVATE (object);

    release_lut_mems (priv);

    g_free (priv->host_sin_lut);
    g_free (priv->host_cos_lut);

    if (priv->nearest_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->nearest_kernel));
        priv->nearest_kernel = NULL;
    }

    if (priv->texture_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->texture_kernel));
        priv->texture_kernel = NULL;
    }

    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }

    G_OBJECT_CLASS (ufo_backproject_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_backproject_task_setup;
    iface->get_requisition = ufo_backproject_task_get_requisition;
    iface->get_num_inputs = ufo_filter_task_get_num_inputs;
    iface->get_num_dimensions = ufo_filter_task_get_num_dimensions;
    iface->get_mode = ufo_filter_task_get_mode;
    iface->process = ufo_backproject_task_process;
}

static void
ufo_backproject_task_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
    UfoBackprojectTaskPrivate *priv = UFO_BACKPROJECT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_PROJECTIONS:
            priv->n_projections = g_value_get_uint (value);
            break;
        case PROP_OFFSET:
            priv->offset = g_value_get_uint (value);
            break;
        case PROP_AXIS_POSITION:
            priv->axis_pos = g_value_get_double (value);
            break;
        case PROP_ANGLE_STEP:
            priv->angle_step = g_value_get_double (value);
            break;
        case PROP_ANGLE_OFFSET:
            priv->angle_offset = g_value_get_double (value);
            priv->luts_changed = TRUE;
            break;
        case PROP_MODE:
            priv->mode = g_value_get_enum (value);
            break;
        case PROP_ROI_X:
            priv->roi_x = g_value_get_uint (value);
            break;
        case PROP_ROI_Y:
            priv->roi_y = g_value_get_uint (value);
            break;
        case PROP_ROI_WIDTH:
            priv->roi_width = g_value_get_uint (value);
            break;
        case PROP_ROI_HEIGHT:
            priv->roi_height = g_value_get_uint (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_backproject_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoBackprojectTaskPrivate *priv = UFO_BACKPROJECT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_PROJECTIONS:
            g_value_set_uint (value, priv->n_projections);
            break;
        case PROP_OFFSET:
            g_value_set_uint (value, priv->offset);
            break;
        case PROP_AXIS_POSITION:
            g_value_set_double (value, priv->axis_pos);
            break;
        case PROP_ANGLE_STEP:
            g_value_set_double (value, priv->angle_step);
            break;
        case PROP_ANGLE_OFFSET:
            g_value_set_double (value, priv->angle_offset);
            break;
        case PROP_MODE:
            g_value_set_enum (value, priv->mode);
            break;
        case PROP_ROI_X:
            g_value_set_uint (value, priv->roi_x);
            break;
        case PROP_ROI_Y:
            g_value_set_uint (value, priv->roi_y);
            break;
        case PROP_ROI_WIDTH:
            g_value_set_uint (value, priv->roi_width);
            break;
        case PROP_ROI_HEIGHT:
            g_value_set_uint (value, priv->roi_height);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_backproject_task_class_init (UfoBackprojectTaskClass *klass)
{
    GObjectClass *oclass;
    UfoNodeClass *node_class;
    
    oclass = G_OBJECT_CLASS (klass);
    node_class = UFO_NODE_CLASS (klass);

    oclass->finalize = ufo_backproject_task_finalize;
    oclass->set_property = ufo_backproject_task_set_property;
    oclass->get_property = ufo_backproject_task_get_property;

    properties[PROP_NUM_PROJECTIONS] =
        g_param_spec_uint ("num-projections",
            "Number of projections between 0 and 180 degrees",
            "Number of projections between 0 and 180 degrees",
            0, +32768, 0,
            G_PARAM_READWRITE);

    properties[PROP_OFFSET] =
        g_param_spec_uint ("offset",
            "Offset to the first projection",
            "Offset to the first projection",
            0, +32768, 0,
            G_PARAM_READWRITE);

    properties[PROP_AXIS_POSITION] =
        g_param_spec_double ("axis-pos",
            "Position of rotation axis",
            "Position of rotation axis",
            -1.0, +32768.0, 0.0,
            G_PARAM_READWRITE);

    properties[PROP_ANGLE_STEP] =
        g_param_spec_double ("angle-step",
            "Increment of angle in radians",
            "Increment of angle in radians",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
            G_PARAM_READWRITE);

    properties[PROP_ANGLE_OFFSET] =
        g_param_spec_double ("angle-offset",
            "Angle offset in radians",
            "Angle offset in radians determining the first angle position",
            0.0, G_MAXDOUBLE, 0.0,
            G_PARAM_READWRITE);

    properties[PROP_MODE] =
        g_param_spec_enum ("mode",
            "Backprojection mode (\"nearest\", \"texture\")",
            "Backprojection mode (\"nearest\", \"texture\")",
            g_enum_register_static ("ufo_backproject_mode", mode_values),
            MODE_TEXTURE, G_PARAM_READWRITE);

    properties[PROP_ROI_X] =
        g_param_spec_uint ("roi-x",
            "X coordinate of region of interest",
            "X coordinate of region of interest",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE);

    properties[PROP_ROI_Y] =
        g_param_spec_uint ("roi-y",
            "Y coordinate of region of interest",
            "Y coordinate of region of interest",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE);

    properties[PROP_ROI_WIDTH] =
        g_param_spec_uint ("roi-width",
            "Width of region of interest",
            "Width of region of interest",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE);

    properties[PROP_ROI_HEIGHT] =
        g_param_spec_uint ("roi-height",
            "Height of region of interest",
            "Height of region of interest",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    node_class->equal = ufo_backproject_task_equal_real;

    g_type_class_add_private(klass, sizeof(UfoBackprojectTaskPrivate));
}

static void
ufo_backproject_task_init (UfoBackprojectTask *self)
{
    UfoBackprojectTaskPrivate *priv;
    self->priv = priv = UFO_BACKPROJECT_TASK_GET_PRIVATE (self);
    priv->nearest_kernel = NULL;
    priv->texture_kernel = NULL;
    priv->n_projections = 0;
    priv->offset = 0;
    priv->axis_pos = -1.0;
    priv->angle_step = -1.0;
    priv->angle_offset = 0.0;
    priv->real_angle_step = -1.0;
    priv->sin_lut = NULL;
    priv->cos_lut = NULL;
    priv->host_sin_lut = NULL;
    priv->host_cos_lut = NULL;
    priv->mode = MODE_TEXTURE;
    priv->luts_changed = TRUE;
    priv->roi_x = priv->roi_y = 0;
    priv->roi_width = priv->roi_height = 0;
}
