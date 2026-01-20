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
#include <stdio.h>
#include <math.h>
#include <glib.h>
#include <glib/gprintf.h>

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "ufo-lamino-backproject-task.h"
#include "lamino-roi.h"
#include "common/ufo-addressing.h"

/* Copy only neccessary projection region */
/* TODO: make this a parameter? */
/* Wait with enabling this until sync issues in ufo-core have been solved */
#define COPY_PROJECTION_REGION 0
#define EXTRACT_FLOAT(region, index) g_value_get_float (g_value_array_get_nth ((region), (index)))
#define REGION_SIZE(region) ((EXTRACT_INT ((region), 2)) == 0) ? 0 : \
                            ((EXTRACT_INT ((region), 1) - EXTRACT_INT ((region), 0) - 1) /\
                            EXTRACT_INT ((region), 2) + 1)
#define PAD_TO_DIVIDE(dividend, divisor) ((dividend) + (divisor) - (dividend) % (divisor))


typedef enum {
    PARAMETER_Z,
    PARAMETER_X_CENTER,
    PARAMETER_LAMINO_ANGLE,
    PARAMETER_ROLL_ANGLE
} Parameter;

static GEnumValue parameter_values[] = {
    { PARAMETER_Z,              "PARAMETER_Z",  "z" },
    { PARAMETER_X_CENTER,       "X_CENTER",     "x-center" },
    { PARAMETER_LAMINO_ANGLE,   "LAMINO_ANGLE", "lamino-angle" },
    { PARAMETER_ROLL_ANGLE,     "ROLL_ANGLE",   "roll-angle" },
    { 0, NULL, NULL}
};

struct _UfoLaminoBackprojectTaskPrivate {
    /* private */
    gboolean generated;
    guint count;
    /* sine and cosine table size based on BURST */
    gsize table_size;

    /* OpenCL */
    cl_context context;
    cl_kernel vector_kernel;
    cl_kernel scalar_kernel;
    cl_sampler sampler;
    /* Buffered images for invoking backprojection on BURST projections at once.
     * We potentially don't need to copy the last image and can use the one from
     * framework directly but it seems to have no performance effects. */
    cl_mem images[BURST];

    /* properties */
    GValueArray *x_region;
    GValueArray *y_region;
    GValueArray *region;
    GValueArray *center;
    GValueArray *projection_offset;
    float sines[BURST], cosines[BURST];
    guint num_projections;
    gfloat overall_angle;
    gfloat tomo_angle;
    gfloat lamino_angle;
    gfloat z;
    gfloat roll_angle;
    Parameter parameter;
    AddressingMode addressing_mode;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoLaminoBackprojectTask, ufo_lamino_backproject_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_LAMINO_BACKPROJECT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_LAMINO_BACKPROJECT_TASK, UfoLaminoBackprojectTaskPrivate))

enum {
    PROP_0,
    PROP_X_REGION,
    PROP_Y_REGION,
    PROP_Z,
    PROP_REGION,
    PROP_PROJECTION_OFFSET,
    PROP_CENTER,
    PROP_NUM_PROJECTIONS,
    PROP_OVERALL_ANGLE,
    PROP_TOMO_ANGLE,
    PROP_LAMINO_ANGLE,
    PROP_PARAMETER,
    PROP_ROLL_ANGLE,
    PROP_ADDRESSING_MODE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void
set_region (GValueArray *src, GValueArray **dst)
{
    if (EXTRACT_INT (src, 0) > EXTRACT_INT (src, 1)) {
        g_warning ("Invalid region [\"from\", \"to\", \"step\"]: [%d, %d, %d], "\
                   "\"from\" has to be less than or equal to \"to\"",
                   EXTRACT_INT (src, 0), EXTRACT_INT (src, 1), EXTRACT_INT (src, 2));
    }
    else {
        g_value_array_free (*dst);
        *dst = g_value_array_copy (src);
    }
}

static void
copy_to_image (UfoBuffer *input,
               cl_mem output_image,
               cl_command_queue cmd_queue,
               size_t origin[3],
               size_t region[3],
               gint in_width)
{
    cl_mem input_data;
    cl_event event;

    input_data = ufo_buffer_get_device_image (input, cmd_queue);
    UFO_RESOURCES_CHECK_CLERR (clEnqueueCopyImage (cmd_queue, input_data, output_image,
                                                   origin, origin, region,
                                                   0, NULL, &event));

    UFO_RESOURCES_CHECK_CLERR (clWaitForEvents (1, &event));
    UFO_RESOURCES_CHECK_CLERR (clReleaseEvent (event));
}

UfoNode *
ufo_lamino_backproject_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_LAMINO_BACKPROJECT_TASK, NULL));
}

static void
ufo_lamino_backproject_task_setup (UfoTask *task,
                                   UfoResources *resources,
                                   GError **error)
{
    UfoLaminoBackprojectTaskPrivate *priv;
    cl_int cl_error;
    gint i;
    gchar *vector_kernel_name;
    gchar *kernel_filename;

    priv = UFO_LAMINO_BACKPROJECT_TASK_GET_PRIVATE (task);

    if (!priv->num_projections) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP, "Number of projections has not been set");
        return;
    }

    if (EXTRACT_FLOAT (priv->region, 2) == 0.0f) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP, "Step in region is 0");
        return;
    }

    vector_kernel_name = g_strdup_printf ("backproject_burst_%d", BURST);

    if (!vector_kernel_name) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP, "Unable to create burst kernel name");
        return;
    }

    priv->context = ufo_resources_get_context (resources);

    switch (priv->parameter) {
        case PARAMETER_Z:
            kernel_filename = g_strdup ("z_kernel.cl");
            break;
        case PARAMETER_X_CENTER:
            kernel_filename = g_strdup ("center_kernel.cl");
            break;
        case PARAMETER_LAMINO_ANGLE:
            kernel_filename = g_strdup ("lamino_kernel.cl");
            break;
        case PARAMETER_ROLL_ANGLE:
            kernel_filename = g_strdup ("roll_kernel.cl");
            break;
        default:
            g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP, "Unknown varying parameter");
            return;
    }

    priv->vector_kernel = ufo_resources_get_kernel (resources, kernel_filename, vector_kernel_name, NULL, error);
    priv->scalar_kernel = ufo_resources_get_kernel (resources, kernel_filename, "backproject_burst_1", NULL, error);
    priv->sampler = clCreateSampler (priv->context, (cl_bool) FALSE, priv->addressing_mode, CL_FILTER_LINEAR, &cl_error);

    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);
    UFO_RESOURCES_CHECK_SET_AND_RETURN (cl_error, error);

    if (priv->vector_kernel)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->vector_kernel), error);

    if (priv->scalar_kernel)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->scalar_kernel), error);

    for (i = 0; i < BURST; i++)
        priv->images[i] = NULL;

    switch (BURST) {
        case 1: priv->table_size = sizeof (cl_float); break;
        case 2: priv->table_size = sizeof (cl_float2); break;
        case 4: priv->table_size = sizeof (cl_float4); break;
        case 8: priv->table_size = sizeof (cl_float8); break;
        case 16: priv->table_size = sizeof (cl_float16); break;
        default: g_warning ("Unsupported vector size"); break;
    }

    g_free (vector_kernel_name);
    g_free (kernel_filename);
}

static void
ufo_lamino_backproject_task_get_requisition (UfoTask *task,
                                             UfoBuffer **inputs,
                                             UfoRequisition *requisition,
                                             GError **error)
{
    UfoLaminoBackprojectTaskPrivate *priv;
    gfloat start, stop, step;

    priv = UFO_LAMINO_BACKPROJECT_TASK_GET_PRIVATE (task);
    start = EXTRACT_FLOAT (priv->region, 0);
    stop = EXTRACT_FLOAT (priv->region, 1);
    step = EXTRACT_FLOAT (priv->region, 2);

    requisition->n_dims = 3;
    requisition->dims[0] = REGION_SIZE (priv->x_region);
    requisition->dims[1] = REGION_SIZE (priv->y_region);
    requisition->dims[2] = (gint) ceil ((stop - start) / step);
}

static guint
ufo_lamino_backproject_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_lamino_backproject_task_get_num_dimensions (UfoTask *task,
                                                guint input)
{
    g_return_val_if_fail (input == 0, 0);

    return 3;
}

static UfoTaskMode
ufo_lamino_backproject_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_REDUCTOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_lamino_backproject_task_process (UfoTask *task,
                                     UfoBuffer **inputs,
                                     UfoBuffer *output,
                                     UfoRequisition *requisition)
{
    UfoLaminoBackprojectTaskPrivate *priv;
    UfoRequisition in_req;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    gfloat tomo_angle, *sines, *cosines;
    gint i, index;
    gint cumulate;
    gsize table_size;
    gboolean scalar;
    /* regions stripped off the "to" value */
    gfloat x_region[2], y_region[2], z_region[2], x_center[2], z_ends[2], lamino_angles[2], roll_angles[2],
           y_center, sin_lamino, cos_lamino, norm_factor, sin_roll, cos_roll;
    gint x_copy_region[2], y_copy_region[2];
    cl_kernel kernel;
    cl_command_queue cmd_queue;
    cl_mem out_mem;
    cl_int cl_error;
    /* image creation and copying */
    cl_image_format image_fmt;
    size_t origin[3];
    size_t region[3];
    /* keep the warp size satisfied but make sure the local grid is localized
     * around a point in 3D for efficient caching */
    const gint real_size[4] = {requisition->dims[0], requisition->dims[1], requisition->dims[2], 0};
    gsize local_work_size[3] = {16, 8, 8};
    gsize global_work_size[3];
    GValue *work_group_size;

    priv = UFO_LAMINO_BACKPROJECT_TASK (task)->priv;
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    work_group_size = ufo_gpu_node_get_info (node, UFO_GPU_NODE_INFO_MAX_WORK_GROUP_SIZE);

    /* Let last axis depend on maximum work group size */
    local_work_size[2] = g_value_get_ulong (work_group_size) / 128;
    g_value_unset (work_group_size);

    global_work_size[0] = requisition->dims[0] % local_work_size[0] ?
                          PAD_TO_DIVIDE (requisition->dims[0], local_work_size[0]) :
                          requisition->dims[0];
    global_work_size[1] = requisition->dims[1] % local_work_size[1] ?
                          PAD_TO_DIVIDE (requisition->dims[1], local_work_size[1]) :
                          requisition->dims[1];
    global_work_size[2] = requisition->dims[2] % local_work_size[2] ?
                          PAD_TO_DIVIDE (requisition->dims[2], local_work_size[2]) :
                          requisition->dims[2];

    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    ufo_buffer_get_requisition (inputs[0], &in_req);

    index = priv->count % BURST;
    tomo_angle = priv->tomo_angle > -G_MAXFLOAT ? priv->tomo_angle :
                 priv->overall_angle * priv->count / priv->num_projections;
    norm_factor = fabs (priv->overall_angle) / priv->num_projections;
    priv->sines[index] = sin (tomo_angle);
    priv->cosines[index] = cos (tomo_angle);
    x_region[0] = (gfloat) EXTRACT_INT (priv->x_region, 0);
    x_region[1] = (gfloat) EXTRACT_INT (priv->x_region, 2);
    y_region[0] = (gfloat) EXTRACT_INT (priv->y_region, 0);
    y_region[1] = (gfloat) EXTRACT_INT (priv->y_region, 2);

    if (priv->parameter == PARAMETER_Z) {
        z_ends[0] = z_region[0] = EXTRACT_FLOAT (priv->region, 0);
        z_region[1] = EXTRACT_FLOAT (priv->region, 2);
        z_ends[1] = EXTRACT_FLOAT (priv->region, 1);
    } else {
        z_ends[0] = z_region[0] = priv->z;
        z_ends[1] = priv->z + 1.0f;
    }

    if (priv->parameter == PARAMETER_X_CENTER) {
        x_center[0] = EXTRACT_FLOAT (priv->region, 0) - EXTRACT_INT (priv->projection_offset, 0);
        x_center[1] = EXTRACT_FLOAT (priv->region, 2);
    } else {
        x_center[0] = x_center[1] = EXTRACT_FLOAT (priv->center, 0) - EXTRACT_INT (priv->projection_offset, 0);
    }

    if (priv->parameter == PARAMETER_LAMINO_ANGLE) {
        lamino_angles[0] = EXTRACT_FLOAT (priv->region, 0);
        lamino_angles[1] = EXTRACT_FLOAT (priv->region, 2);
    } else {
        lamino_angles[0] = lamino_angles[1] = priv->lamino_angle;
    }

    if (priv->parameter == PARAMETER_ROLL_ANGLE) {
        roll_angles[0] = EXTRACT_FLOAT (priv->region, 0);
        roll_angles[1] = EXTRACT_FLOAT (priv->region, 2);
    } else {
        roll_angles[0] = roll_angles[1] = priv->roll_angle;
    }

    y_center = EXTRACT_FLOAT (priv->center, 1) - EXTRACT_INT (priv->projection_offset, 1);
    sin_lamino = sinf (priv->lamino_angle);
    cos_lamino = cosf (priv->lamino_angle);
    /* Minus the value because we are rotating back */
    sin_roll = sinf (-priv->roll_angle);
    cos_roll = cosf (-priv->roll_angle);
    scalar = priv->count >= priv->num_projections / BURST * BURST ? 1 : 0;

    /* If COPY_PROJECTION_REGION is True we copy only the part necessary  */
    /* for a given tomographic and laminographic angle */
    /* TODO: Extend the region determination to be able to handle PARAMETER_LAMINO_ANGLE */
    if (COPY_PROJECTION_REGION && priv->parameter != PARAMETER_LAMINO_ANGLE) {
        determine_x_region (x_copy_region, priv->x_region, priv->y_region, tomo_angle,
                            EXTRACT_FLOAT (priv->center, 0), in_req.dims[0]);
        determine_y_region (y_copy_region, priv->x_region, priv->y_region, z_ends,
                            tomo_angle, priv->lamino_angle, EXTRACT_FLOAT (priv->center, 1),
                            in_req.dims[1]);
        origin[0] = x_copy_region[0];
        origin[1] = y_copy_region[0];
        origin[2] = 0;
        region[0] = x_copy_region[1] - x_copy_region[0];
        region[1] = y_copy_region[1] - y_copy_region[0];
    } else {
        origin[0] = origin[1] = origin[2] = 0;
        region[0] = in_req.dims[0];
        region[1] = in_req.dims[1];
    }
    region[2] = 1;

    if (priv->images[index] == NULL) {
        /* TODO: dangerous, don't rely on the ufo-buffer */
        image_fmt.image_channel_order = CL_INTENSITY;
        image_fmt.image_channel_data_type = CL_FLOAT;
        /* TODO: what with the "other" API? */
        priv->images[index] = clCreateImage2D (priv->context,
                                               CL_MEM_READ_ONLY,
                                               &image_fmt,
                                               in_req.dims[0],
                                               in_req.dims[1],
                                               0,
                                               NULL,
                                               &cl_error);
        UFO_RESOURCES_CHECK_CLERR (cl_error);
    }

    copy_to_image (inputs[0], priv->images[index], cmd_queue, origin, region, in_req.dims[0]);

    if (scalar) {
        kernel = priv->scalar_kernel;
        cumulate = priv->count;
        table_size = sizeof (cl_float);
        sines = &priv->sines[index];
        cosines = &priv->cosines[index];
        i = 1;
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 0, sizeof (cl_mem), &priv->images[index]));
    } else {
        kernel = priv->vector_kernel;
        cumulate = priv->count + 1 == BURST ? 0 : 1;
        table_size = priv->table_size;
        sines = priv->sines;
        cosines = priv->cosines;
        i = BURST;
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, index, sizeof (cl_mem), &priv->images[index]));
    }

    if (scalar || index == BURST - 1) {
        /* Execute the kernel after BURST images have arrived, i.e. we use more
         * projections at one invocation, so the number of read/writes to the
         * result is reduced by a factor of BURST. If there are not enough
         * projecttions left, execute the scalar kernel */
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (cl_mem), &out_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (cl_sampler), &priv->sampler));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (cl_int3), real_size));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (cl_float2), x_center));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (cl_float), (cl_float *) &y_center));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (cl_float2), x_region));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (cl_float2), y_region));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (cl_float2), z_region));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (cl_float2), lamino_angles));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (cl_float2), roll_angles));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (cl_float), &sin_lamino));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (cl_float), &cos_lamino));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, table_size, sines));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, table_size, cosines));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (cl_float), &norm_factor));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (cl_float), &sin_roll));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (cl_float), &cos_roll));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i, sizeof (cl_int), (cl_int *) &cumulate));

        profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
        ufo_profiler_call (profiler, cmd_queue, kernel, 3, global_work_size, local_work_size);
    }

    priv->count++;

    return TRUE;
}

static gboolean
ufo_lamino_backproject_task_generate (UfoTask *task,
                                      UfoBuffer *output,
                                      UfoRequisition *requisition)
{
    UfoLaminoBackprojectTaskPrivate *priv;

    priv = UFO_LAMINO_BACKPROJECT_TASK_GET_PRIVATE (task);

    if (priv->generated) {
        return FALSE;
    }

    priv->generated = TRUE;

    return TRUE;
}

static void
ufo_lamino_backproject_task_set_property (GObject *object,
                                          guint property_id,
                                          const GValue *value,
                                          GParamSpec *pspec)
{
    UfoLaminoBackprojectTaskPrivate *priv = UFO_LAMINO_BACKPROJECT_TASK_GET_PRIVATE (object);
    GValueArray *array;

    switch (property_id) {
        case PROP_X_REGION:
            array = (GValueArray *) g_value_get_boxed (value);
            set_region (array, &priv->x_region);
            break;
        case PROP_Y_REGION:
            array = (GValueArray *) g_value_get_boxed (value);
            set_region (array, &priv->y_region);
            break;
        case PROP_Z:
            priv->z = g_value_get_float (value);
            break;
        case PROP_REGION:
            array = (GValueArray *) g_value_get_boxed (value);
            g_value_array_free (priv->region);
            priv->region = g_value_array_copy (array);
            break;
        case PROP_PROJECTION_OFFSET:
            array = (GValueArray *) g_value_get_boxed (value);
            g_value_array_free (priv->projection_offset);
            priv->projection_offset = g_value_array_copy (array);
            break;
        case PROP_CENTER:
            array = (GValueArray *) g_value_get_boxed (value);
            g_value_array_free (priv->center);
            priv->center = g_value_array_copy (array);
            break;
        case PROP_NUM_PROJECTIONS:
            priv->num_projections = g_value_get_uint (value);
            break;
        case PROP_OVERALL_ANGLE:
            priv->overall_angle = g_value_get_float (value);
            break;
        case PROP_TOMO_ANGLE:
            priv->tomo_angle = g_value_get_float (value);
            break;
        case PROP_LAMINO_ANGLE:
            priv->lamino_angle = g_value_get_float (value);
            break;
        case PROP_ROLL_ANGLE:
            priv->roll_angle = g_value_get_float (value);
            break;
        case PROP_PARAMETER:
            priv->parameter = g_value_get_enum (value);
            break;
        case PROP_ADDRESSING_MODE:
            priv->addressing_mode = g_value_get_enum (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_lamino_backproject_task_get_property (GObject *object,
                                          guint property_id,
                                          GValue *value,
                                          GParamSpec *pspec)
{
    UfoLaminoBackprojectTaskPrivate *priv = UFO_LAMINO_BACKPROJECT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_X_REGION:
            g_value_set_boxed (value, priv->x_region);
            break;
        case PROP_Y_REGION:
            g_value_set_boxed (value, priv->y_region);
            break;
        case PROP_Z:
            g_value_set_float (value, priv->z);
            break;
        case PROP_REGION:
            g_value_set_boxed (value, priv->region);
            break;
        case PROP_PROJECTION_OFFSET:
            g_value_set_boxed (value, priv->projection_offset);
            break;
        case PROP_CENTER:
            g_value_set_boxed (value, priv->center);
            break;
        case PROP_NUM_PROJECTIONS:
            g_value_set_uint (value, priv->num_projections);
            break;
        case PROP_OVERALL_ANGLE:
            g_value_set_float (value, priv->overall_angle);
            break;
        case PROP_TOMO_ANGLE:
            g_value_set_float (value, priv->tomo_angle);
            break;
        case PROP_LAMINO_ANGLE:
            g_value_set_float (value, priv->lamino_angle);
            break;
        case PROP_ROLL_ANGLE:
            g_value_set_float (value, priv->roll_angle);
            break;
        case PROP_PARAMETER:
            g_value_set_enum (value, priv->parameter);
            break;
        case PROP_ADDRESSING_MODE:
            g_value_set_enum (value, priv->addressing_mode);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_lamino_backproject_task_finalize (GObject *object)
{
    UfoLaminoBackprojectTaskPrivate *priv;
    gint i;

    priv = UFO_LAMINO_BACKPROJECT_TASK_GET_PRIVATE (object);
    g_value_array_free (priv->x_region);
    g_value_array_free (priv->y_region);
    g_value_array_free (priv->region);
    g_value_array_free (priv->projection_offset);
    g_value_array_free (priv->center);

    if (priv->vector_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->vector_kernel));
        priv->vector_kernel = NULL;
    }
    if (priv->scalar_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->scalar_kernel));
        priv->scalar_kernel = NULL;
    }
    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }
    if (priv->sampler) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseSampler (priv->sampler));
        priv->sampler = NULL;
    }

    for (i = 0; i < BURST; i++) {
        if (priv->images[i] != NULL) {
            UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->images[i]));
            priv->images[i] = NULL;
        }
    }

    G_OBJECT_CLASS (ufo_lamino_backproject_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_lamino_backproject_task_setup;
    iface->get_num_inputs = ufo_lamino_backproject_task_get_num_inputs;
    iface->get_num_dimensions = ufo_lamino_backproject_task_get_num_dimensions;
    iface->get_mode = ufo_lamino_backproject_task_get_mode;
    iface->get_requisition = ufo_lamino_backproject_task_get_requisition;
    iface->process = ufo_lamino_backproject_task_process;
    iface->generate = ufo_lamino_backproject_task_generate;
}

static void
ufo_lamino_backproject_task_class_init (UfoLaminoBackprojectTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_lamino_backproject_task_set_property;
    oclass->get_property = ufo_lamino_backproject_task_get_property;
    oclass->finalize = ufo_lamino_backproject_task_finalize;

    GParamSpec *region_vals = g_param_spec_int ("region-values",
                                                "Region values",
                                                "Elements in regions",
                                                G_MININT,
                                                G_MAXINT,
                                                (gint) 0,
                                                G_PARAM_READWRITE);

    GParamSpec *float_region_vals = g_param_spec_float ("float-region-values",
                                                        "Float Region values",
                                                        "Elements in float regions",
                                                        -G_MAXFLOAT,
                                                        G_MAXFLOAT,
                                                        0.0f,
                                                        G_PARAM_READWRITE);

    properties[PROP_X_REGION] =
        g_param_spec_value_array ("x-region",
            "X region for reconstruction as (from, to, step)",
            "X region for reconstruction as (from, to, step)",
            region_vals,
            G_PARAM_READWRITE);

    properties[PROP_Y_REGION] =
        g_param_spec_value_array ("y-region",
            "Y region for reconstruction as (from, to, step)",
            "Y region for reconstruction as (from, to, step)",
            region_vals,
            G_PARAM_READWRITE);

    properties[PROP_Z] =
        g_param_spec_float ("z",
            "Z coordinate of the reconstructed slice",
            "Z coordinate of the reconstructed slice",
            -G_MAXFLOAT, G_MAXFLOAT, 0.0f,
            G_PARAM_READWRITE);

    properties[PROP_REGION] =
        g_param_spec_value_array ("region",
            "Region for the parameter along z-axis as (from, to, step)",
            "Region for the parameter along z-axis as (from, to, step)",
            float_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_PROJECTION_OFFSET] =
        g_param_spec_value_array ("projection-offset",
            "Offset to projection data as (x, y)",
            "Offset to projection data as (x, y) for the case input data "
                "is cropped to the necessary range of interest",
            region_vals,
            G_PARAM_READWRITE);

    properties[PROP_CENTER] =
        g_param_spec_value_array ("center",
            "Center of the volume with respect to projections (x, y)",
            "Center of the volume with respect to projections (x, y), (rotation axes)",
            float_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_OVERALL_ANGLE] =
        g_param_spec_float ("overall-angle",
            "Angle covered by all projections",
            "Angle covered by all projections (can be negative for negative steps "
            "in case only num-projections is specified",
            -G_MAXFLOAT, G_MAXFLOAT, G_PI,
            G_PARAM_READWRITE);

    properties[PROP_NUM_PROJECTIONS] =
        g_param_spec_uint ("num-projections",
            "Number of projections",
            "Number of projections",
            0, 32768, 0,
            G_PARAM_READWRITE);

    properties[PROP_TOMO_ANGLE] =
        g_param_spec_float ("tomo-angle",
            "Tomographic rotation angle in radians",
            "Tomographic rotation angle in radians (used for acquiring projections)",
            -G_MAXFLOAT, G_MAXFLOAT, 0.0f,
            G_PARAM_READWRITE);

    properties[PROP_LAMINO_ANGLE] =
        g_param_spec_float ("lamino-angle",
            "Absolute laminogrpahic angle in radians",
            "Absolute laminogrpahic angle in radians determining the sample tilt",
            -G_MAXFLOAT, G_MAXFLOAT, 0.0f,
            G_PARAM_READWRITE);

    properties[PROP_ROLL_ANGLE] =
        g_param_spec_float ("roll-angle",
            "Sample angular misalignment to the side (roll) in radians",
            "Sample angular misalignment to the side (roll) in radians (CW is positive)",
            -G_MAXFLOAT, G_MAXFLOAT, 0.0f,
            G_PARAM_READWRITE);

    properties[PROP_PARAMETER] =
        g_param_spec_enum ("parameter",
            "Which parameter will be varied along the z-axis",
            "Which parameter will be varied along the z-axis "
                "(\"z\", \"x-center\", \"lamino-angle\", \"roll-angle\")",
            g_enum_register_static ("ufo_lbp_parameter", parameter_values),
            PARAMETER_Z,
            G_PARAM_READWRITE);

    properties[PROP_ADDRESSING_MODE] =
        g_param_spec_enum ("addressing-mode",
            "Outlier treatment (\"none\", \"clamp\", \"clamp_to_edge\", \"repeat\")",
            "Outlier treatment (\"none\", \"clamp\", \"clamp_to_edge\", \"repeat\")",
            g_enum_register_static ("ufo_lbp_addressing_mode", addressing_values),
            CL_ADDRESS_CLAMP,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoLaminoBackprojectTaskPrivate));
}

static void
ufo_lamino_backproject_task_init(UfoLaminoBackprojectTask *self)
{
    self->priv = UFO_LAMINO_BACKPROJECT_TASK_GET_PRIVATE(self);
    guint i;
    GValue int_zero = G_VALUE_INIT;
    GValue float_zero = G_VALUE_INIT;

    g_value_init (&int_zero, G_TYPE_INT);
    g_value_init (&float_zero, G_TYPE_FLOAT);
    g_value_set_int (&int_zero, 0);
    g_value_set_float (&float_zero, 0.0f);
    self->priv->x_region = g_value_array_new (3);
    self->priv->y_region = g_value_array_new (3);
    self->priv->region = g_value_array_new (3);
    self->priv->z = 0.0f;
    self->priv->projection_offset = g_value_array_new (2);
    self->priv->center = g_value_array_new (2);

    for (i = 0; i < 3; i++) {
        g_value_array_insert (self->priv->x_region, i, &int_zero);
        g_value_array_insert (self->priv->y_region, i, &int_zero);
        g_value_array_insert (self->priv->region, i, &float_zero);
        if (i < 2) {
            g_value_array_insert (self->priv->projection_offset, i, &int_zero);
            g_value_array_insert (self->priv->center, i, &float_zero);
        }
    }

    self->priv->num_projections = 0;
    self->priv->overall_angle = G_PI;
    self->priv->tomo_angle = -G_MAXFLOAT;
    self->priv->lamino_angle = 0.0f;
    self->priv->roll_angle = 0.0f;
    self->priv->parameter = PARAMETER_Z;
    self->priv->count = 0;
    self->priv->addressing_mode = CL_ADDRESS_CLAMP;
    self->priv->generated = FALSE;
}
