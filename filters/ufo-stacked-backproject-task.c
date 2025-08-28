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

#include <stdio.h>
#include <math.h>
#include "ufo-stacked-backproject-task.h"

typedef enum {
    INT8,
    HALF,
    SINGLE
} Precision;

static GEnumValue precision_values[] = {
        {INT8,"INT8","int8"},
        {HALF, "HALF", "half"},
        {SINGLE, "SINGLE", "single"},
        { 0, NULL, NULL}
};

struct _UfoStackedBackprojectTaskPrivate {
    cl_context context;
    cl_kernel interleave_single;
    cl_kernel interleave_half;
    cl_kernel interleave_uint;
    cl_kernel uninterleave_single;
    cl_kernel uninterleave_half;
    cl_kernel uninterleave_uint;
    cl_kernel texture_single;
    cl_kernel texture_half;
    cl_kernel texture_uint;
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
    Precision precision;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoStackedBackprojectTask, ufo_stacked_backproject_task, UFO_TYPE_TASK_NODE,
        G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                               ufo_task_interface_init))

#define UFO_STACKED_BACKPROJECT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_STACKED_BACKPROJECT_TASK, UfoStackedBackprojectTaskPrivate))

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
    PROP_PRECISION,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_stacked_backproject_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_STACKED_BACKPROJECT_TASK, NULL));
}

static void
ufo_stacked_backproject_task_setup (UfoTask *task,
                                    UfoResources *resources,
                                    GError **error)
{
    UfoStackedBackprojectTaskPrivate *priv;

    priv = UFO_STACKED_BACKPROJECT_TASK_GET_PRIVATE(task);

    priv->context = ufo_resources_get_context(resources);

    priv->interleave_single = ufo_resources_get_kernel (resources, "stacked-backproject.cl", "interleave_single", NULL, error);
    priv->texture_single = ufo_resources_get_kernel (resources, "stacked-backproject.cl", "texture_single", NULL, error);
    priv->uninterleave_single = ufo_resources_get_kernel (resources, "stacked-backproject.cl", "uninterleave_single", NULL, error);

    priv->interleave_half = ufo_resources_get_kernel (resources, "stacked-backproject.cl", "interleave_half", NULL, error);
    priv->texture_half = ufo_resources_get_kernel (resources, "stacked-backproject.cl", "texture_half", NULL, error);
    priv->uninterleave_half = ufo_resources_get_kernel (resources, "stacked-backproject.cl", "uninterleave_half", NULL, error);

    priv->interleave_uint = ufo_resources_get_kernel (resources, "stacked-backproject.cl", "interleave_uint", NULL, error);
    priv->texture_uint = ufo_resources_get_kernel (resources, "stacked-backproject.cl", "texture_uint", NULL, error);
    priv->uninterleave_uint = ufo_resources_get_kernel (resources, "stacked-backproject.cl", "uninterleave_uint", NULL, error);

    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);

    if (priv->interleave_single != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->interleave_single), error);

    if (priv->interleave_half != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->interleave_half), error);

    if (priv->interleave_uint != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->interleave_uint), error);

    if (priv->uninterleave_single != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->uninterleave_single), error);

    if (priv->uninterleave_half != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->uninterleave_half), error);

    if (priv->uninterleave_uint != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->uninterleave_uint), error);

    if (priv->texture_single != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->texture_single), error);

    if (priv->texture_half != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->texture_half), error);

    if (priv->texture_uint != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->texture_uint), error);
}

static cl_mem
create_lut_buffer (UfoStackedBackprojectTaskPrivate *priv,
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
release_lut_mems (UfoStackedBackprojectTaskPrivate *priv)
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
ufo_stacked_backproject_task_get_requisition (UfoTask *task,
                                              UfoBuffer **inputs,
                                              UfoRequisition *requisition,
                                              GError **error)
{
    UfoStackedBackprojectTaskPrivate *priv;
    UfoRequisition in_req;

    priv = UFO_STACKED_BACKPROJECT_TASK_GET_PRIVATE(task);
    ufo_buffer_get_requisition(inputs[0],&in_req);

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

    requisition->n_dims = 3;

    /* TODO: we should check here, that we might access data outside the projections */

    requisition->dims[0] = priv->roi_width == 0 ? in_req.dims[0] : (gsize) priv->roi_width;
    requisition->dims[1] = priv->roi_height == 0 ? in_req.dims[0] : (gsize) priv->roi_height;
    requisition->dims[2] = in_req.n_dims == 3 ? in_req.dims[2]:1;

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
ufo_stacked_backproject_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_stacked_backproject_task_get_num_dimensions (UfoTask *task,
                                                 guint input)
{
    g_return_val_if_fail (input == 0, 0);
    return 3;
}

static UfoTaskMode
ufo_stacked_backproject_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_stacked_backproject_task_equal_real (UfoNode *n1,
                                         UfoNode *n2)
{
    g_return_val_if_fail (UFO_IS_STACKED_BACKPROJECT_TASK (n1) && UFO_IS_STACKED_BACKPROJECT_TASK (n2), FALSE);
    return UFO_STACKED_BACKPROJECT_TASK (n1)->priv->texture_single == UFO_STACKED_BACKPROJECT_TASK (n2)->priv->texture_single;
}


static gboolean
ufo_stacked_backproject_task_process (UfoTask *task,
                                      UfoBuffer **inputs,
                                      UfoBuffer *output,
                                      UfoRequisition *requisition)
{
    UfoStackedBackprojectTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    cl_mem interleaved_img;
    cl_mem out_mem;
    cl_mem reconstructed_buffer;
    cl_mem device_array;

    cl_kernel kernel_interleave;
    cl_kernel kernel_texture;
    cl_kernel kernel_uninterleave;

    size_t buffer_size;
    gfloat axis_pos;

    priv = UFO_STACKED_BACKPROJECT_TASK(task)->priv;
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node(UFO_TASK_NODE(task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue(node);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));

    /* Guess axis position if they are not provided by the user. */
    if (priv->axis_pos <= 0.0) {
        UfoRequisition in_req;

        ufo_buffer_get_requisition(inputs[0], &in_req);
        axis_pos = (gfloat) ((gfloat) in_req.dims[0]) / 2.0f;
    } else {
        axis_pos = priv->axis_pos;
    }

    // Image format
    cl_image_format format;
    device_array = ufo_buffer_get_device_array(inputs[0],cmd_queue);

    UfoRequisition req;
    ufo_buffer_get_requisition(inputs[0],&req);

    unsigned long dim_x = (requisition->dims[0]%16 == 0) ? requisition->dims[0] : (((requisition->dims[0]/16)+1)*16);
    unsigned long dim_y = (requisition->dims[1]%16 == 0) ? requisition->dims[1] : (((requisition->dims[1]/16)+1)*16);
    unsigned long quotient;

    // reconstructs in 3 precision modes
    // 2 slices are reconstructed for single precision, else reconstructs 4 slices in parallel
    if(priv->precision == SINGLE){
        quotient = requisition->dims[2]/2;
        kernel_interleave = priv->interleave_single;
        kernel_texture = priv->texture_single;
        kernel_uninterleave = priv->uninterleave_single;
        format.image_channel_order = CL_RG;
        format.image_channel_data_type = CL_FLOAT;
        buffer_size = sizeof(cl_float2) * dim_x * dim_y * quotient;
    }else if(priv->precision == HALF){
        quotient = requisition->dims[2]/4;
        kernel_interleave = priv->interleave_half;
        kernel_texture = priv->texture_half;
        kernel_uninterleave = priv->uninterleave_half;
        format.image_channel_order = CL_RGBA;
        buffer_size = sizeof(cl_float4) * dim_x * dim_y * quotient;
        format.image_channel_data_type = CL_HALF_FLOAT;
    }else if(priv->precision == INT8){
        quotient = requisition->dims[2]/4;
        kernel_interleave = priv->interleave_uint;
        kernel_texture = priv->texture_uint;
        kernel_uninterleave = priv->uninterleave_uint;
        format.image_channel_order = CL_RGBA;
        format.image_channel_data_type = CL_UNSIGNED_INT8;
        buffer_size = sizeof(cl_uint4) * dim_x * dim_y * quotient;
    }

    cl_image_desc imageDesc;
    imageDesc.image_width = req.dims[0];
    imageDesc.image_height = req.dims[1];
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = quotient;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    imageDesc.image_slice_pitch = 0;
    imageDesc.image_row_pitch = 0;
    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.buffer = NULL;

    float max_element;
    float min_element;

    if(quotient > 0) {
        // Interleave
        interleaved_img = clCreateImage(priv->context, CL_MEM_READ_WRITE, &format, &imageDesc, NULL, 0);

        UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_interleave, 0, sizeof(cl_mem), &device_array));
        UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_interleave, 1, sizeof(cl_mem), &interleaved_img));
        if(priv->precision == INT8){
            //Normalize i.e convert float array to 0-255
            float *host = ufo_buffer_get_host_array(inputs[0],cmd_queue);
            min_element = ufo_buffer_min(inputs[0],cmd_queue);
            max_element = ufo_buffer_max(inputs[0],cmd_queue);

            UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_interleave, 2, sizeof(float), &min_element));
            UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_interleave, 3, sizeof(float), &max_element));
        }

        size_t gsize_interleave[3] = {req.dims[0],req.dims[1],quotient};
        ufo_profiler_call(profiler, cmd_queue, kernel_interleave, 3, gsize_interleave, NULL);

        // SINOGRAM RECONSTRUCTION FOR MULTIPLE SLICES
        reconstructed_buffer = clCreateBuffer(priv->context, CL_MEM_READ_WRITE, buffer_size, NULL, 0);

        UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_texture, 0, sizeof(cl_mem), &interleaved_img));
        UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_texture, 1, sizeof(cl_mem), &reconstructed_buffer));
        UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_texture, 2, sizeof(cl_mem), &priv->sin_lut));
        UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_texture, 3, sizeof(cl_mem), &priv->cos_lut));
        UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_texture, 4, sizeof(guint), &priv->roi_x));
        UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_texture, 5, sizeof(guint), &priv->roi_y));
        UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_texture, 6, sizeof(guint), &priv->offset));
        UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_texture, 7, sizeof(guint), &priv->burst_projections));
        UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_texture, 8, sizeof(gfloat), &axis_pos));
        UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_texture, 9, sizeof(unsigned long), &requisition->dims[0]));

        size_t gsize_texture[3] = {dim_x,dim_y,quotient};
        size_t lSize[3] = {16,16,1};
        ufo_profiler_call(profiler, cmd_queue, kernel_texture, 3, gsize_texture, lSize);

        //UNINTERLEAVE
        UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_uninterleave, 0, sizeof(cl_mem), &reconstructed_buffer));
        UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_uninterleave, 1, sizeof(cl_mem), &out_mem));
        if(priv->precision == INT8){
            UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_uninterleave, 2, sizeof(float), &min_element));
            UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_uninterleave, 3, sizeof(float), &max_element));
            UFO_RESOURCES_CHECK_CLERR(clSetKernelArg(kernel_uninterleave, 4, sizeof(guint), &priv->burst_projections));
        }

        size_t gsize_uninterleave[3] = {requisition->dims[0],requisition->dims[1],quotient};
        ufo_profiler_call(profiler, cmd_queue, kernel_uninterleave, 3, gsize_uninterleave, NULL);

        UFO_RESOURCES_CHECK_CLERR(clReleaseMemObject(interleaved_img));
        UFO_RESOURCES_CHECK_CLERR(clReleaseMemObject(reconstructed_buffer));
    }

    return TRUE;
}


static void
ufo_stacked_backproject_task_set_property (GObject *object,
                                           guint property_id,
                                           const GValue *value,
                                           GParamSpec *pspec)
{
    UfoStackedBackprojectTaskPrivate *priv = UFO_STACKED_BACKPROJECT_TASK_GET_PRIVATE (object);

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
        case PROP_PRECISION:
            priv->precision = g_value_get_enum(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_stacked_backproject_task_get_property (GObject *object,
                                           guint property_id,
                                           GValue *value,
                                           GParamSpec *pspec)
{
    UfoStackedBackprojectTaskPrivate *priv = UFO_STACKED_BACKPROJECT_TASK_GET_PRIVATE (object);

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
        case PROP_PRECISION:
            g_value_set_enum(value,priv->precision);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_stacked_backproject_task_finalize (GObject *object)
{
    UfoStackedBackprojectTaskPrivate *priv;
    priv = UFO_STACKED_BACKPROJECT_TASK_GET_PRIVATE(object);

    release_lut_mems(priv);

    g_free(priv->host_sin_lut);
    g_free(priv->host_cos_lut);

    if (priv->interleave_single) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->interleave_single));
        priv->interleave_single = NULL;
    }

    if (priv->interleave_half) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->interleave_half));
        priv->interleave_half = NULL;
    }

    if (priv->interleave_uint) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->interleave_uint));
        priv->interleave_uint = NULL;
    }

    if (priv->uninterleave_single) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->uninterleave_single));
        priv->uninterleave_single = NULL;
    }

    if (priv->uninterleave_half) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->uninterleave_half));
        priv->uninterleave_half = NULL;
    }

    if (priv->uninterleave_uint) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->uninterleave_uint));
        priv->uninterleave_uint = NULL;
    }

    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }

    if (priv->texture_single) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->texture_single));
        priv->texture_single = NULL;
    }

    if (priv->texture_half) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->texture_half));
        priv->texture_half = NULL;
    }

    if (priv->texture_uint) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->texture_uint));
        priv->texture_uint = NULL;
    }

    G_OBJECT_CLASS (ufo_stacked_backproject_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_stacked_backproject_task_setup;
    iface->get_num_inputs = ufo_stacked_backproject_task_get_num_inputs;
    iface->get_num_dimensions = ufo_stacked_backproject_task_get_num_dimensions;
    iface->get_mode = ufo_stacked_backproject_task_get_mode;
    iface->get_requisition = ufo_stacked_backproject_task_get_requisition;
    iface->process = ufo_stacked_backproject_task_process;
}


static void
ufo_stacked_backproject_task_class_init (UfoStackedBackprojectTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);
    UfoNodeClass *node_class;

    oclass = G_OBJECT_CLASS (klass);
    node_class = UFO_NODE_CLASS (klass);

    oclass->set_property = ufo_stacked_backproject_task_set_property;
    oclass->get_property = ufo_stacked_backproject_task_get_property;
    oclass->finalize = ufo_stacked_backproject_task_finalize;

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

    properties[PROP_PRECISION] =
            g_param_spec_enum("precision-mode",
                              "Precision mode (\"int8\", \"half\", \"single\")",
                              "Precision mode (\"int8\", \"half\", \"single\")",
                              g_enum_register_static("ufo_stacked_backproject_precision", precision_values),
                              SINGLE, G_PARAM_READWRITE);


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

    node_class->equal = ufo_stacked_backproject_task_equal_real;

    g_type_class_add_private (oclass, sizeof(UfoStackedBackprojectTaskPrivate));
}

static void
ufo_stacked_backproject_task_init(UfoStackedBackprojectTask *self)
{
    UfoStackedBackprojectTaskPrivate *priv;
    self->priv = priv = UFO_STACKED_BACKPROJECT_TASK_GET_PRIVATE(self);
    priv->interleave_single = NULL;
    priv->interleave_half = NULL;
    priv->interleave_uint = NULL;
    priv->uninterleave_single = NULL;
    priv->uninterleave_half = NULL;
    priv->uninterleave_uint = NULL;
    priv->texture_single = NULL;
    priv->texture_half = NULL;
    priv->texture_uint = NULL;
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
    priv->luts_changed = TRUE;
    priv->roi_x = priv->roi_y = 0;
    priv->roi_width = priv->roi_height = 0;
    priv->precision = SINGLE;
}