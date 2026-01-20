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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <config.h>
#include <common/ufo-math.h>
#include "common/ufo-conebeam.h"
#include "common/ufo-scarray.h"
#include "common/ufo-ctgeometry.h"
#include "common/ufo-addressing.h"
#include "ufo-general-backproject-task.h"

#define NUM_VECTOR_ARGUMENTS 11
#define REAL_SIZE_ARG_INDEX 1
#define STATIC_ARG_OFFSET 18
#define G_LOG_LEVEL_DOMAIN "gbp"
#define REGION_SIZE(region) (ceil ((ufo_scarray_get_double ((region), 1) - ufo_scarray_get_double ((region), 0)) /\
                             ufo_scarray_get_double ((region), 2)))
#define NEXT_DIVISOR(dividend, divisor) ((dividend) + (divisor) - (dividend) % (divisor))
#define DEFINE_FILL_SINCOS(type)                      \
static void                                           \
fill_sincos_##type (type *array, const gdouble angle) \
{                                                     \
    array[0] = (type) sin (angle);                    \
    array[1] = (type) cos (angle);                    \
}

#define DEFINE_CREATE_REGIONS(type)                                                   \
static void                                                                           \
create_regions_##type (UfoGeneralBackprojectTaskPrivate *priv,                        \
                       const cl_command_queue cmd_queue,                              \
                       const gdouble start,                                           \
                       const gdouble step)                                            \
{                                                                                     \
    guint i, j;                                                                       \
    gsize region_size;                                                                \
    type *region_values;                                                              \
    cl_int cl_error;                                                                  \
    gdouble value;                                                                    \
    gboolean is_angular = is_parameter_angular (priv->parameter);                     \
                                                                                      \
    g_log ("gbp", G_LOG_LEVEL_DEBUG, "Start, step: %g %g", start, step);              \
                                                                                      \
    region_size = priv->num_slices_per_chunk * 2 * sizeof (type);                     \
    region_values = (type *) g_malloc0 (region_size);                                 \
                                                                                      \
    for (i = 0; i < priv->num_chunks; i++) {                                          \
        g_log ("gbp", G_LOG_LEVEL_DEBUG, "Chunk %d region:", i);                      \
        for (j = 0; j < priv->num_slices_per_chunk; j++) {                            \
            value = start + i * priv->num_slices_per_chunk * step + j * step;         \
            if (is_angular) {                                                         \
                region_values[2 * j] = (type) sin (value);                            \
                region_values[2 * j + 1] = (type) cos (value);                        \
            } else {                                                                  \
                region_values[2 * j] = (type) value;                                  \
            }                                                                         \
        }                                                                             \
        priv->cl_regions[i] = clCreateBuffer (priv->context,                          \
                                              CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,\
                                              region_size,                            \
                                              region_values,                          \
                                              &cl_error);                             \
        UFO_RESOURCES_CHECK_CLERR (cl_error);                                         \
    }                                                                                 \
                                                                                      \
    g_free (region_values);                                                           \
}

#define DEFINE_TRANSFER_ANGULAR_ARGUMENT(type)                                          \
static cl_mem                                                                           \
transfer_angular_argument_##type (UfoGeneralBackprojectTaskPrivate *priv,               \
                                  UfoScarray *source)                                   \
{                                                                                       \
    gsize size;                                                                         \
    guint i;                                                                            \
    cl_mem device_array;                                                                \
    type *host_array;                                                                   \
                                                                                        \
    size = 2 * priv->num_projections * sizeof (type);                                   \
    if (!(host_array = (type *) g_malloc (size))) {                                     \
        g_warning ("Error allocating vectorized parameter host memory");                \
        return NULL;                                                                    \
    }                                                                                   \
                                                                                        \
    for (i = 0; i < priv->num_projections; i++) {                                       \
        fill_sincos_##type (host_array + 2 * i, ufo_scarray_get_double (source, i));    \
    }                                                                                   \
                                                                                        \
    device_array = transfer_host_to_device (priv->context, host_array, size);           \
    g_free (host_array);                                                                \
                                                                                        \
    return device_array;                                                                \
}                                                                                       \

#define DEFINE_TRANSFER_POSITINAL_ARGUMENT(type)                                \
static cl_mem                                                                   \
transfer_positional_argument_##type (UfoGeneralBackprojectTaskPrivate *priv,    \
                                     UfoScpoint *source)                        \
{                                                                               \
    gsize size;                                                                 \
    guint i;                                                                    \
    type *host_array;                                                           \
    cl_mem device_array;                                                        \
                                                                                \
    size = 4 * priv->num_projections * sizeof (type);                           \
    if (!(host_array = (type *) g_malloc (size))) {                             \
        g_warning ("Error allocating vectorized parameter host memory");        \
        return NULL;                                                            \
    }                                                                           \
                                                                                \
    for (i = 0; i < priv->num_projections; i++) {                               \
        host_array[4 * i] = (type) ufo_scarray_get_double (source->x, i);       \
        host_array[4 * i + 1] = (type) ufo_scarray_get_double (source->y, i);   \
        host_array[4 * i + 2] = (type) ufo_scarray_get_double (source->z, i);   \
    }                                                                           \
                                                                                \
    device_array = transfer_host_to_device (priv->context, host_array, size);   \
    g_free (host_array);                                                        \
                                                                                \
    return device_array;                                                        \
}

#define DEFINE_SET_ANGULAR_VECTOR_KERNEL_ARGUMENT(type)                                                                     \
static void                                                                                                                 \
set_angular_vector_kernel_argument_##type (UfoGeneralBackprojectTaskPrivate *priv,                                          \
                                           cl_kernel kernel,                                                                \
                                           UfoScpoint *point,                                                               \
                                           gint mem_index,                                                                  \
                                           gint arg_index)                                                                  \
{                                                                                                                           \
    priv->vector_arguments[mem_index] = transfer_angular_argument_##type (priv, point->x);                                  \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index++, sizeof (cl_mem), &priv->vector_arguments[mem_index++]));\
    priv->vector_arguments[mem_index] = transfer_angular_argument_##type (priv, point->y);                                  \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index++, sizeof (cl_mem), &priv->vector_arguments[mem_index++]));\
    priv->vector_arguments[mem_index] = transfer_angular_argument_##type (priv, point->z);                                  \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index, sizeof (cl_mem), &priv->vector_arguments[mem_index]));    \
}

#define DEFINE_SET_STATIC_VECTOR_ARGUMENTS(type)                                                                            \
static gint                                                                                                                 \
set_static_vector_arguments_##type (UfoTask *task,                                                                          \
                                    cl_kernel kernel,                                                                       \
                                    gint arg_index)                                                                         \
{                                                                                                                           \
    UfoGeneralBackprojectTaskPrivate *priv;                                                                                 \
    gint mem_index;                                                                                                         \
                                                                                                                            \
    priv = UFO_GENERAL_BACKPROJECT_TASK_GET_PRIVATE (task);                                                                 \
    priv->vector_arguments = (cl_mem *) g_malloc (NUM_VECTOR_ARGUMENTS * sizeof (cl_mem));                                  \
                                                                                                                            \
    /* Axis angle has only two vector components, the z one is the tomographic angle with offset set in process () */       \
    priv->vector_arguments[0] = transfer_angular_argument_##type (priv, priv->geometry->axis->angle->x);                    \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index++, sizeof (cl_mem), &priv->vector_arguments[0]));          \
    priv->vector_arguments[1] = transfer_angular_argument_##type (priv, priv->geometry->axis->angle->y);                    \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index++, sizeof (cl_mem), &priv->vector_arguments[1]));          \
                                                                                                                            \
    set_angular_vector_kernel_argument_##type (priv, kernel, priv->geometry->volume_angle, 2, arg_index);                   \
    set_angular_vector_kernel_argument_##type (priv, kernel, priv->geometry->detector->angle, 5, arg_index + 3);            \
    mem_index = 8;                                                                                                          \
    arg_index += 6;                                                                                                         \
    priv->vector_arguments[mem_index] = transfer_positional_argument_##type (priv, priv->geometry->axis->position);         \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index++, sizeof (cl_mem), &priv->vector_arguments[mem_index++]));\
    priv->vector_arguments[mem_index] = transfer_positional_argument_##type (priv, priv->geometry->source_position);        \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index++, sizeof (cl_mem), &priv->vector_arguments[mem_index++]));\
    priv->vector_arguments[mem_index] = transfer_positional_argument_##type (priv, priv->geometry->detector->position);     \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index++, sizeof (cl_mem), &priv->vector_arguments[mem_index++]));\
                                                                                                                            \
    return arg_index;                                                                                                       \
}

#define DEFINE_SET_STATIC_SCALAR_ARGUMENTS(type)                                                                            \
static gint                                                                                                                 \
set_static_scalar_arguments_##type (UfoTask *task,                                                                          \
                                    cl_kernel kernel,                                                                       \
                                    gint arg_index)                                                                         \
{                                                                                                                           \
    UfoGeneralBackprojectTaskPrivate *priv;                                                                                 \
    type axis_angle_x[2], axis_angle_y[2], axis_angle_z[2], volume_angle_x[2], volume_angle_y[2], volume_angle_z[2],        \
         detector_angle_x[2], detector_angle_y[2], detector_angle_z[2], center_position[4], source_position[4],             \
         detector_position[4];                                                                                              \
    guint count;                                                                                                            \
    priv = UFO_GENERAL_BACKPROJECT_TASK_GET_PRIVATE (task);                                                                 \
    g_object_get (task, "num_processed", &count, NULL);                                                                     \
                                                                                                                            \
    fill_sincos_##type (axis_angle_x, ufo_scarray_get_double (priv->geometry->axis->angle->x, count));                      \
    fill_sincos_##type (axis_angle_y, ufo_scarray_get_double (priv->geometry->axis->angle->y, count));                      \
    fill_sincos_##type (axis_angle_z, ufo_scarray_get_double (priv->geometry->axis->angle->z, count));                      \
    fill_sincos_##type (volume_angle_x, ufo_scarray_get_double (priv->geometry->volume_angle->x, count));                   \
    fill_sincos_##type (volume_angle_y, ufo_scarray_get_double (priv->geometry->volume_angle->y, count));                   \
    fill_sincos_##type (volume_angle_z, ufo_scarray_get_double (priv->geometry->volume_angle->z, count));                   \
    fill_sincos_##type (detector_angle_x, ufo_scarray_get_double (priv->geometry->detector->angle->x, count));              \
    fill_sincos_##type (detector_angle_y, ufo_scarray_get_double (priv->geometry->detector->angle->y, count));              \
    fill_sincos_##type (detector_angle_z, ufo_scarray_get_double (priv->geometry->detector->angle->z, count));              \
    center_position[0] = (type) ufo_scarray_get_double (priv->geometry->axis->position->x, count);                          \
    center_position[2] = (type) ufo_scarray_get_double (priv->geometry->axis->position->z, count);                          \
    /* TODO: use only 2D center in the kernel */                                                                            \
    center_position[1] = 0.0f;                                                                                              \
    source_position[0] = (type) ufo_scarray_get_double (priv->geometry->source_position->x, count);                         \
    source_position[1] = (type) ufo_scarray_get_double (priv->geometry->source_position->y, count);                         \
    source_position[2] = (type) ufo_scarray_get_double (priv->geometry->source_position->z, count);                         \
    detector_position[0] = (type) ufo_scarray_get_double (priv->geometry->detector->position->x, count);                    \
    detector_position[1] = (type) ufo_scarray_get_double (priv->geometry->detector->position->y, count);                    \
    detector_position[2] = (type) ufo_scarray_get_double (priv->geometry->detector->position->z, count);                    \
                                                                                                                            \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index++, sizeof (type##2), axis_angle_x));                       \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index++, sizeof (type##2), axis_angle_y));                       \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index++, sizeof (type##2), volume_angle_x));                     \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index++, sizeof (type##2), volume_angle_y));                     \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index++, sizeof (type##2), volume_angle_z));                     \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index++, sizeof (type##2), detector_angle_x));                   \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index++, sizeof (type##2), detector_angle_y));                   \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index++, sizeof (type##2), detector_angle_z));                   \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index++, sizeof (type##3), center_position));                    \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index++, sizeof (type##3), source_position));                    \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, arg_index++, sizeof (type##3), detector_position));                  \
                                                                                                                            \
    g_log ("gbp", G_LOG_LEVEL_DEBUG, "axis: %g %g, %g %g, %g %g",                                                           \
           axis_angle_x[0], axis_angle_x[1], axis_angle_y[0], axis_angle_y[1], axis_angle_z[0], axis_angle_z[1]);           \
    g_log ("gbp", G_LOG_LEVEL_DEBUG, "volume: %g %g, %g %g, %g %g",                                                         \
           volume_angle_x[0], volume_angle_x[1], volume_angle_y[0], volume_angle_y[1], volume_angle_z[0],                   \
           volume_angle_z[1]);                                                                                              \
    g_log ("gbp", G_LOG_LEVEL_DEBUG, "detector_x: %g %g, %g %g, %g %g",                                                     \
           detector_angle_x[0], detector_angle_x[1], detector_angle_y[0], detector_angle_y[1], detector_angle_z[0],         \
           detector_angle_z[1]);                                                                                            \
    g_log ("gbp", G_LOG_LEVEL_DEBUG, "center_position: %g %g %g",                                                           \
           center_position[0], center_position[1], center_position[2]);                                                     \
    g_log ("gbp", G_LOG_LEVEL_DEBUG, "source_position: %g %g %g",                                                           \
           source_position[0], source_position[1], source_position[2]);                                                     \
    g_log ("gbp", G_LOG_LEVEL_DEBUG, "detector_position: %g %g %g",                                                         \
           detector_position[0], detector_position[1], detector_position[2]);                                               \
    return arg_index;                                                                                                       \
}

#define DEFINE_COMPUTE_SLICE_REGION(type)                                                                                   \
static void                                                                                                                 \
compute_slice_region_##type (UfoGeneralBackprojectTaskPrivate *priv,                                                        \
                             gsize length,                                                                                  \
                             UfoScarray *region,                                                                            \
                             type region_for_opencl[2])                                                                     \
{                                                                                                                           \
    if (ufo_scarray_get_double (region, 2)) {                                                                               \
        region_for_opencl[0] = (type) ufo_scarray_get_double (region, 0);                                                   \
        region_for_opencl[1] = (type) ufo_scarray_get_double (region, 2);                                                   \
    } else {                                                                                                                \
        region_for_opencl[0] = -((type) length / 2);                                                                        \
        region_for_opencl[1] = (type) 1;                                                                                    \
    }                                                                                                                       \
}

#define DEFINE_SET_STATIC_ARGS(type)                                                                                     \
static void                                                                                                              \
set_static_args_##type (UfoTask *task,                                                                                   \
                        UfoRequisition *requisition,                                                                     \
                        const cl_kernel kernel)                                                                          \
{                                                                                                                        \
    UfoGeneralBackprojectTaskPrivate *priv;                                                                              \
    gdouble gray_delta_recip;                                                                                            \
    type slice_z_position, region_x[2], region_y[2], gray_limit[2], norm_factor;                                         \
    /* 0 = sampler, 1 = real size set in process (), thus we start with 2 */                                             \
    guint burst, j, i = 2;                                                                                               \
    priv = UFO_GENERAL_BACKPROJECT_TASK_GET_PRIVATE (task);                                                              \
    gray_delta_recip = (gdouble) get_integer_maximum (st_values[priv->store_type].value_nick) /                          \
                               (priv->gray_map_max - priv->gray_map_min);                                                \
    norm_factor = fabs (priv->overall_angle) / priv->num_projections;                                                    \
    burst = kernel == priv->kernel ? priv->burst : priv->num_projections % priv->burst;                                  \
                                                                                                                         \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, 0, sizeof (cl_sampler), &priv->sampler));                         \
                                                                                                                         \
    compute_slice_region_##type (priv, requisition->dims[0], priv->region_x, region_x);                                  \
    compute_slice_region_##type (priv, requisition->dims[1], priv->region_y, region_y);                                  \
    slice_z_position = (type) priv->z;                                                                                   \
    norm_factor = (type) norm_factor;                                                                                    \
    gray_limit[0] = (type) priv->gray_map_min;                                                                           \
    gray_limit[1] = (type) gray_delta_recip;                                                                             \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (type##2), region_x));                                \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (type##2), region_y));                                \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (type), &slice_z_position));                          \
    if (priv->vectorized) {                                                                                              \
        i = set_static_vector_arguments_##type (task, kernel, i);                                                        \
    } else {                                                                                                             \
        i = set_static_scalar_arguments_##type (task, kernel, i);                                                        \
    }                                                                                                                    \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (type), &norm_factor));                               \
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (type##2), gray_limit));                              \
    g_log ("gbp", G_LOG_LEVEL_DEBUG, "region_x: %g %g", region_x[0], region_x[1]);                                       \
    g_log ("gbp", G_LOG_LEVEL_DEBUG, "region_y: %g %g", region_y[0], region_y[1]);                                       \
    g_log ("gbp", G_LOG_LEVEL_DEBUG, "slice_z_position: %g", slice_z_position);                                          \
    g_log ("gbp", G_LOG_LEVEL_DEBUG, "norm_factor: %g", norm_factor);                                                    \
    g_log ("gbp", G_LOG_LEVEL_DEBUG, "gray_limit: %g %g", gray_limit[0], gray_limit[1]);                                 \
                                                                                                                         \
    for (j = 0; j < burst; j++) {                                                                                        \
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, i++, sizeof (cl_mem), &priv->projections[j]));                \
    }                                                                                                                    \
}

/*{{{ Enumerations */
typedef enum {
    FT_HALF,
    FT_FLOAT,
    FT_DOUBLE
} FloatType;

typedef enum {
    CT_FLOAT,
    CT_DOUBLE
} ComputeType;

typedef enum {
    ST_HALF,
    ST_FLOAT,
    ST_DOUBLE,
    ST_UCHAR,
    ST_USHORT,
    ST_UINT
} StoreType;

static const GEnumValue parameter_values[] = {
    { UFO_UNI_RECO_PARAMETER_AXIS_ROTATION_X,     "UFO_UNI_RECO_PARAMETER_AXIS_ROTATION_X",     "axis-angle-x" },
    { UFO_UNI_RECO_PARAMETER_AXIS_ROTATION_Y,     "UFO_UNI_RECO_PARAMETER_AXIS_ROTATION_Y",     "axis-angle-y" },
    { UFO_UNI_RECO_PARAMETER_AXIS_ROTATION_Z,     "UFO_UNI_RECO_PARAMETER_AXIS_ROTATION_Z",     "axis-angle-z" },
    { UFO_UNI_RECO_PARAMETER_VOLUME_ROTATION_X,   "UFO_UNI_RECO_PARAMETER_VOLUME_ROTATION_X",   "volume-angle-x" },
    { UFO_UNI_RECO_PARAMETER_VOLUME_ROTATION_Y,   "UFO_UNI_RECO_PARAMETER_VOLUME_ROTATION_Y",   "volume-angle-y" },
    { UFO_UNI_RECO_PARAMETER_VOLUME_ROTATION_Z,   "UFO_UNI_RECO_PARAMETER_VOLUME_ROTATION_Z",   "volume-angle-z" },
    { UFO_UNI_RECO_PARAMETER_DETECTOR_ROTATION_X, "UFO_UNI_RECO_PARAMETER_DETECTOR_ROTATION_X", "detector-angle-x" },
    { UFO_UNI_RECO_PARAMETER_DETECTOR_ROTATION_Y, "UFO_UNI_RECO_PARAMETER_DETECTOR_ROTATION_Y", "detector-angle-y" },
    { UFO_UNI_RECO_PARAMETER_DETECTOR_ROTATION_Z, "UFO_UNI_RECO_PARAMETER_DETECTOR_ROTATION_Z", "detector-angle-z" },
    { UFO_UNI_RECO_PARAMETER_DETECTOR_POSITION_X, "UFO_UNI_RECO_PARAMETER_DETECTOR_POSITION_X", "detector-position-x" },
    { UFO_UNI_RECO_PARAMETER_DETECTOR_POSITION_Y, "UFO_UNI_RECO_PARAMETER_DETECTOR_POSITION_Y", "detector-position-y" },
    { UFO_UNI_RECO_PARAMETER_DETECTOR_POSITION_Z, "UFO_UNI_RECO_PARAMETER_DETECTOR_POSITION_Z", "detector-position-z" },
    { UFO_UNI_RECO_PARAMETER_SOURCE_POSITION_X,   "UFO_UNI_RECO_PARAMETER_SOURCE_POSITION_X",   "source-position-x" },
    { UFO_UNI_RECO_PARAMETER_SOURCE_POSITION_Y,   "UFO_UNI_RECO_PARAMETER_SOURCE_POSITION_Y",   "source-position-y" },
    { UFO_UNI_RECO_PARAMETER_SOURCE_POSITION_Z,   "UFO_UNI_RECO_PARAMETER_SOURCE_POSITION_Z",   "source-position-z" },
    { UFO_UNI_RECO_PARAMETER_CENTER_POSITION_X,   "UFO_UNI_RECO_PARAMETER_CENTER_POSITION_X",   "center-position-x" },
    { UFO_UNI_RECO_PARAMETER_CENTER_POSITION_Z,   "UFO_UNI_RECO_PARAMETER_CENTER_POSITION_Z",   "center-position-z" },
    { UFO_UNI_RECO_PARAMETER_Z,                   "UFO_UNI_RECO_PARAMETER_Z",         "z" },
    { 0, NULL, NULL}
};

static GEnumValue compute_type_values[] = {
    {CT_FLOAT,  "CT_FLOAT",  "float"},
    {CT_DOUBLE, "CT_DOUBLE", "double"},
    { 0, NULL, NULL}
};

static GEnumValue ft_values[] = {
    {FT_HALF,   "FT_HALF",   "half"},
    {FT_FLOAT,  "FT_FLOAT",  "float"},
    {FT_DOUBLE, "FT_DOUBLE", "double"},
    { 0, NULL, NULL}
};

static GEnumValue st_values[] = {
    {ST_HALF,   "ST_HALF",   "half"},
    {ST_FLOAT,  "ST_FLOAT",  "float"},
    {ST_DOUBLE, "ST_DOUBLE", "double"},
    {ST_UCHAR,  "ST_UCHAR",  "uchar"},
    {ST_USHORT, "ST_USHORT", "ushort"},
    {ST_UINT,   "ST_UINT",   "uint"},
    { 0, NULL, NULL}
};
/*}}}*/

struct _UfoGeneralBackprojectTaskPrivate {
    /* Properties */
    guint burst;
    gdouble z;
    UfoScarray *region, *region_x, *region_y;
    UfoCTGeometry *geometry;
    ComputeType compute_type, result_type;
    StoreType store_type;
    UfoUniRecoParameter parameter;
    gdouble gray_map_min, gray_map_max;
    /* Private */
    gboolean vectorized;
    guint generated;
    UfoResources *resources;
    cl_mem *projections;
    cl_mem *chunks;
    cl_mem *cl_regions, *vector_arguments;
    guint num_slices, num_slices_per_chunk, num_chunks;
    guint num_projections;
    gdouble overall_angle;
    AddressingMode addressing_mode;
    GHashTable *node_props_table;
    /* OpenCL */
    cl_context context;
    cl_kernel kernel, rest_kernel;
    cl_sampler sampler;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoGeneralBackprojectTask, ufo_general_backproject_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_GENERAL_BACKPROJECT_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GENERAL_BACKPROJECT_TASK, UfoGeneralBackprojectTaskPrivate))

enum {
    PROP_0,
    PROP_BURST,
    PROP_PARAMETER,
    PROP_Z,
    PROP_REGION,
    PROP_REGION_X,
    PROP_REGION_Y,
    PROP_CENTER_POSITION_X,
    PROP_CENTER_POSITION_Z,
    PROP_SOURCE_POSITION_X,
    PROP_SOURCE_POSITION_Y,
    PROP_SOURCE_POSITION_Z,
    PROP_DETECTOR_POSITION_X,
    PROP_DETECTOR_POSITION_Y,
    PROP_DETECTOR_POSITION_Z,
    PROP_DETECTOR_ANGLE_X,
    PROP_DETECTOR_ANGLE_Y,
    PROP_DETECTOR_ANGLE_Z,
    PROP_AXIS_ANGLE_X,
    PROP_AXIS_ANGLE_Y,
    PROP_AXIS_ANGLE_Z,
    PROP_VOLUME_ANGLE_X,
    PROP_VOLUME_ANGLE_Y,
    PROP_VOLUME_ANGLE_Z,
    PROP_NUM_PROJECTIONS,
    PROP_COMPUTE_TYPE,
    PROP_RESULT_TYPE,
    PROP_STORE_TYPE,
    PROP_OVERALL_ANGLE,
    PROP_ADDRESSING_MODE,
    PROP_GRAY_MAP_MIN,
    PROP_GRAY_MAP_MAX,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

/*{{{ General helper functions*/
DEFINE_FILL_SINCOS (cl_float)
DEFINE_FILL_SINCOS (cl_double)

static gsize
get_type_size (StoreType type)
{
    gsize size;

    switch (type) {
        case ST_HALF:
            size = sizeof (cl_half);
            break;
        case ST_FLOAT:
            size = sizeof (cl_float);
            break;
        case ST_DOUBLE:
            size = sizeof (cl_double);
            break;
        case ST_UCHAR:
            size = sizeof (cl_uchar);
            break;
        case ST_USHORT:
            size = sizeof (cl_ushort);
            break;
        case ST_UINT:
            size = sizeof (cl_uint);
            break;
        default:
            g_warning ("Uknown store type");
            size = 0;
            break;
    }

    return size;
}

static gulong
get_integer_maximum (const gchar *type_name)
{
    if (!g_strcmp0 (type_name, "uchar"))
        return 0xFF;

    if (!g_strcmp0 (type_name, "ushort"))
        return 0xFFFF;

    if (!g_strcmp0 (type_name, "uint"))
        return 0xFFFFFFFF;

    return 0;
}

static gboolean
is_axis_parameter (UfoUniRecoParameter parameter)
{
    return parameter == UFO_UNI_RECO_PARAMETER_AXIS_ROTATION_X ||
           parameter == UFO_UNI_RECO_PARAMETER_AXIS_ROTATION_Y ||
           parameter == UFO_UNI_RECO_PARAMETER_AXIS_ROTATION_Z;
}

static gboolean
is_volume_parameter (UfoUniRecoParameter parameter)
{
    return parameter == UFO_UNI_RECO_PARAMETER_VOLUME_ROTATION_X ||
           parameter == UFO_UNI_RECO_PARAMETER_VOLUME_ROTATION_Y ||
           parameter == UFO_UNI_RECO_PARAMETER_VOLUME_ROTATION_Z;
}

static gboolean
is_detector_rotation_parameter (UfoUniRecoParameter parameter)
{
    return parameter == UFO_UNI_RECO_PARAMETER_DETECTOR_ROTATION_X ||
           parameter == UFO_UNI_RECO_PARAMETER_DETECTOR_ROTATION_Y ||
           parameter == UFO_UNI_RECO_PARAMETER_DETECTOR_ROTATION_Z;
}

static gboolean
is_center_position_parameter (UfoUniRecoParameter parameter)
{
    return parameter == UFO_UNI_RECO_PARAMETER_CENTER_POSITION_X ||
           parameter == UFO_UNI_RECO_PARAMETER_CENTER_POSITION_Z;
}

static gboolean
is_source_position_parameter (UfoUniRecoParameter parameter)
{
    return parameter == UFO_UNI_RECO_PARAMETER_SOURCE_POSITION_X ||
           parameter == UFO_UNI_RECO_PARAMETER_SOURCE_POSITION_Y ||
           parameter == UFO_UNI_RECO_PARAMETER_SOURCE_POSITION_Z;
}

static gboolean
is_detector_position_parameter (UfoUniRecoParameter parameter)
{
    return parameter == UFO_UNI_RECO_PARAMETER_DETECTOR_POSITION_X ||
           parameter == UFO_UNI_RECO_PARAMETER_DETECTOR_POSITION_Y ||
           parameter == UFO_UNI_RECO_PARAMETER_DETECTOR_POSITION_Z;
}

static gboolean
is_parameter_positional (UfoUniRecoParameter parameter)
{
    return is_center_position_parameter (parameter) ||
           is_source_position_parameter (parameter) ||
           is_detector_position_parameter (parameter);
}

static gboolean
is_parameter_angular (UfoUniRecoParameter parameter)
{
    return is_axis_parameter (parameter) ||
           is_volume_parameter (parameter) ||
           is_detector_rotation_parameter (parameter);
}
/*}}}*/

/*{{{ String Helper functions*/
static gchar *
replace_substring (const gchar *haystack, const gchar *needle, const gchar *replacement)
{
    GRegex *regex;
    gchar *result;

    regex = g_regex_new (needle, 0, 0, NULL);
    result = g_regex_replace_literal (regex, haystack, -1, 0, replacement, 0, NULL);
    g_regex_unref (regex);
    return result;
}
/*}}}*/

/*{{{ Kernel creation*/
static gchar *
replace_parameter_dashes (UfoUniRecoParameter parameter)
{
    return replace_substring (parameter_values[parameter].value_nick, "-", "_");
}

static gchar *
get_kernel_parameter_name (UfoUniRecoParameter parameter)
{
    gchar **entries, *result, *param_kernel_name;
    param_kernel_name = replace_parameter_dashes (parameter);

    if (is_parameter_positional (parameter)) {
        entries = g_strsplit (param_kernel_name, "_", 3);
        if (g_strcmp0 (entries[0], param_kernel_name)) {
            result = g_strconcat (entries[0], "_", entries[1], NULL);
        }
        g_strfreev (entries);
    } else {
        result = g_strdup (param_kernel_name);
    }
    g_free (param_kernel_name);

    return result;
}

static gchar *
make_template (UfoGeneralBackprojectTaskPrivate *priv)
{
    gchar *template, *definitions, *header, *header_1, *body, *kernel_parameter_name, *tmp;

    if (!(definitions = ufo_resources_get_kernel_source (priv->resources, "general_bp_definitions.in", NULL))) {
        g_warning ("Error obtaining general backprojection kernel header template");
        return NULL;
    }
    if (!(body = ufo_resources_get_kernel_source (priv->resources, "general_bp_body.in", NULL))) {
        g_warning ("Error obtaining general backprojection kernel body template");
        return NULL;
    }
    if (priv->vectorized) {
        if (!(header = ufo_resources_get_kernel_source (priv->resources, "general_bp_header_vector.in", NULL))) {
            g_warning ("Error obtaining general backprojection kernel header template");
            return NULL;
        }
        if (priv->parameter != UFO_UNI_RECO_PARAMETER_Z) {
            kernel_parameter_name = get_kernel_parameter_name (priv->parameter);
            tmp = g_strconcat (kernel_parameter_name, "_global", NULL);
            header_1 = replace_substring (header, kernel_parameter_name, tmp);
            g_free (tmp);
            g_free (header);
            header = header_1;
        }
        header_1 = replace_substring (header, "%memspace%", "global ");
        g_free (header);
        header = header_1;
    } else {
        if (!(header = ufo_resources_get_kernel_source (priv->resources, "general_bp_header_scalar.in", NULL))) {
            g_warning ("Error obtaining general backprojection kernel header template");
            return NULL;
        }
    }
    template = g_strconcat (definitions, header, body, NULL);
    g_free (definitions);
    g_free (header);
    g_free (body);

    return template;
}

/**
 * make_args:
 * @burst: (in): number of processed projections in the kernel
 * @fmt: (in): format string which will be transformed to the projection index
 *
 * Make kernel arguments.
 */
static gchar *
make_args (gint burst, const gchar *fmt)
{
    gint i;
    gulong size, written;
    gchar *one, *str, *ptr;

    size = strlen (fmt) + 1;
    one = g_strnfill (size, 0);
    str = g_strnfill (burst * size, 0);
    ptr = str;

    for (i = 0; i < burst; i++) {
        written = g_snprintf (one, size, fmt, i);
        if (written > size) {
            g_free (one);
            g_free (str);
            return NULL;
        }
        ptr = g_stpcpy (ptr, one);
    }
    g_free (one);

    return str;
}

/**
 * make_type_conversion:
 * @compute_type: (in): data type for internal computations
 * @store_type: (in): output volume data type
 *
 * Make conversions necessary for computation and output data types.
 */
static gchar *
make_type_conversion (const gchar *compute_type, const gchar *store_type)
{
    gchar *code;
    gulong maxval = get_integer_maximum (store_type);

    if (maxval) {
        code = g_strdup_printf ("(%s) clamp ((%s)(gray_limit.y * (norm_factor * result - gray_limit.x)), (%s) 0.0, (%s) %lu.0)",
                                store_type, compute_type, compute_type, compute_type, maxval);
    } else {
        code = g_strdup_printf ("(%s) (norm_factor * result)", store_type);
    }

    return code;
}

/**
 * make_parameter_initial_assignment:
 * @parameter: (in): parameter which represents the third reconstruction axis
 *
 * Make initial parameter assignment for vectorized kernels which need to first
 * copy the global values to a private variable.
 */
static gchar *
make_parameter_initial_assignment (UfoUniRecoParameter parameter)
{
    gchar *code = NULL, *kernel_parameter_name;

    if (parameter == UFO_UNI_RECO_PARAMETER_Z) {
        code = g_strdup ("");
    } else {
        kernel_parameter_name = get_kernel_parameter_name (parameter);
        if (is_parameter_positional (parameter)) {
            code = g_strconcat ("cfloat3 ", kernel_parameter_name, ";\n", NULL);
        } else if (is_parameter_angular (parameter)) {
            code = g_strconcat ("cfloat2 ", kernel_parameter_name, ";\n", NULL);
        }
        g_free (kernel_parameter_name);
    }

    return code;
}

/**
 * make_parameter_assignment:
 * @parameter: (in): parameter which represents the third reconstruction axis
 *
 * Make parameter assignment.
 */
static gchar *
make_parameter_assignment (UfoUniRecoParameter parameter)
{
    gchar **entries, *param_kernel_name;
    gchar *code = NULL;
    param_kernel_name = replace_parameter_dashes (parameter);

    if (parameter == UFO_UNI_RECO_PARAMETER_Z) {
        code = g_strdup ("\tvoxel_0.z = region[idz].x;\n");
    } else if (is_parameter_positional (parameter)) {
        entries = g_strsplit (param_kernel_name, "_", 3);
        if (g_strcmp0 (entries[0], param_kernel_name)) {
            code = g_strconcat ("\t", entries[0], "_", entries[1], ".", entries[2], " = region[idz].x;\n", NULL);
        }
        g_strfreev (entries);
    } else if (is_parameter_angular (parameter)) {
        code = g_strconcat ("\t", param_kernel_name, " = region[idz];\n", NULL);
    }
    g_free (param_kernel_name);

    return code;
}

/**
 * make_volume_transformation:
 * @values: (in): sine and cosine angle values
 * @point: 3D point which will be rotated
 * @suffix: suffix which comes after values
 *
 * Inplace point rotation about the three coordinate axes.
 */
static gchar *
make_volume_transformation (const gchar *values, const gchar *point, const gchar *suffix)
{
    return g_strdup_printf ("\t%s = rotate_z (%s_z%s, %s);"
                            "\n\t%s = rotate_y (%s_y%s, %s);"
                            "\n\t%s = rotate_x (%s_x%s, %s);\n",
                            point, values, suffix, point,
                            point, values, suffix, point,
                            point, values, suffix, point);
}

/**
 * make_static_transformations:
 * @vectorized (in): geometry parameters are vectors
 * @with_volume: (in): rotate reconstructed volume
 * @perpendicular_detector: (in): is the detector perpendicular to the beam
 * @shifted_detector: (in): is the detector laterally shifted
 * @shifted_source: (in): is the source laterally shifted
 * @parallel_beam: (in): is the beam parallel
 *
 * Make static transformations independent from the tomographic rotation angle.
 */
static gchar *
make_static_transformations (gboolean vectorized, gboolean with_volume,
                             gboolean perpendicular_detector, gboolean shifted_detector,
                             gboolean shifted_source, gboolean parallel_beam)
{
    gchar *code = g_strnfill (8192, 0);
    gchar *current = code;
    gchar *detector_transformation, *volume_transformation;
    const gchar *voxel_0 = vectorized ? "voxel" : "voxel_0";

    if (parallel_beam) {
        if (vectorized) {
            current = g_stpcpy (current,"\tvoxel = voxel_0;\n");
        }
    } else {
        if (vectorized) {
            current = g_stpcpy (current, "\t// Magnification\n\tvoxel = voxel_0 * -native_divide(source_position[%d].y, "
                                "(detector_position[%d].y - source_position[%d].y));\n");
        } else {
            current = g_stpcpy (current, "// Magnification\n\tvoxel_0 *= -native_divide(source_position.y, "
                                "(detector_position.y - source_position.y));\n");
        }
    }
    if (!perpendicular_detector) {
        detector_transformation = make_volume_transformation ("detector_angle", "detector_normal", "[%d]");
        current = g_stpcpy (current, "\tdetector_normal = (float3)(0.0, -1.0, 0.0);\n");
        current = g_stpcpy (current, detector_transformation);
        current = g_stpcpy (current, "\n\tdetector_offset = -dot (detector_position[%d], detector_normal);\n");
        g_free (detector_transformation);
    } else if (!parallel_beam) {
        current = g_stpcpy (current, "\n\tproject_tmp = detector_position[%d].y - source_position[%d].y;\n");
    }
    if (with_volume) {
        volume_transformation = make_volume_transformation ("volume_angle", voxel_0, "[%d]");
        current = g_stpcpy (current, volume_transformation);
        g_free (volume_transformation);
    }
    if (!(perpendicular_detector || parallel_beam)) {
        current = g_stpcpy (current,
                            "\n\ttmp_transformation = "
                            "- (detector_offset + dot (source_position[%d], detector_normal));\n");
    }

    return code;
}

/**
 * make_projection_computation:
 * @perpendicular_detector: (in): is the detector perpendicular to the beam
 * @shifted_detector: (in): is the detector laterally shifted
 * @shifted_source: (in): is the source laterally shifted
 * @parallel_beam: (in): is the beam parallel
 *
 * Make voxel projection calculation with the least possible operations based on
 * geometry settings.
 */
static const gchar *
make_projection_computation (gboolean perpendicular_detector, gboolean shifted_detector,
                             gboolean shifted_source, gboolean parallel_beam)
{
    const gchar *code;

    if (perpendicular_detector) {
        if (parallel_beam) {
            code = "\t// Perpendicular detector in combination with parallel beam geometry, i.e.\n"
                   "\t// voxel.xz is directly the detector coordinate, no transformation necessary\n";
        } else {
            if (shifted_source) {
                code = "\tvoxel = mad (native_divide (project_tmp, (voxel.y - source_position[%d].y)), (voxel - source_position[%d]), source_position[%d]);\n";
            } else {
                code = "\tvoxel = mad (native_divide (project_tmp, (voxel.y - source_position[%d].y)), voxel, source_position[%d]);\n";
            }
        }
    } else {
        if (parallel_beam) {
            code = "\tvoxel.y = -native_divide (mad (voxel.z, detector_normal.z, "
                   "mad (voxel.x, detector_normal.x, detector_offset)), detector_normal.y);\n";
        } else {
            code = "\tvoxel -= source_position[%d];\n"
                   "\tvoxel = mad (native_divide (tmp_transformation, dot (voxel, detector_normal)), voxel, source_position[%d]);\n";
        }
    }

    return code;
}

/**
 * make_transformations:
 * @parameter: parameter reconstructed along the third dimension
 * @vectorized (in): geometry parameters are vectors
 * @burst (in): number of projections processed by the kernel
 * @with_axis: (in): do computations related with rotation axis
 * @perpendicular_detector: (in): is the detector perpendicular to the the mean
 * @shifted_detector: (in): is the detector laterally shifted
 * @shifted_source: (in): is the source laterally shifted
 * beam direction
 * @parallel_beam: (in): is the beam parallel
 * @compute_type: (in): data type for internal computations
 *
 * Make voxel projection calculation with the least possible operations based on
 * geometry settings.
 */
static gchar *
make_transformations (UfoUniRecoParameter parameter, gboolean vectorized, gint burst, gboolean with_volume,
                      gboolean with_axis, gboolean perpendicular_detector, gboolean shifted_detector,
                      gboolean shifted_source, gboolean parallel_beam, const gchar *compute_type)
{
    gint i;
    size_t written = 0;
    gchar *code_fmt, *code, *current, *volume_transformation, *pretransformation, *pixel_reading,
          *str_iteration, *parameter_assignment, *kernel_parameter_name, *tmp, *tmp_2, *str_index;
    const guint snippet_size = 16384;
    const guint size = burst * snippet_size;
    /* Based on eq. 30 from "Direct cone beam SPECT reconstruction with camera tilt" */
    const gchar *slice_coefficient =
        "\t// Get the value and weigh it (source_position is negative, so -voxel.y\n"
        "\tcoeff = native_divide (source_position[%d].y - detector_position[%d].y, (source_position[%d].y - voxel.y));\n";
    const gchar *detector_shift = "\tvoxel -= detector_position[%d];\n";
    const gchar *detector_rotation =
        "\tvoxel = rotate_x ((cfloat2)(-detector_angle_x[%d].x, detector_angle_x[%d].y), voxel);\n"
        "\tvoxel = rotate_y ((cfloat2)(-detector_angle_y[%d].x, detector_angle_y[%d].y), voxel);\n"
        "\tvoxel = rotate_z ((cfloat2)(-detector_angle_z[%d].x, detector_angle_z[%d].y), voxel);\n";

    code_fmt = g_strnfill (snippet_size, 0);
    code = g_strnfill (size, 0);
    kernel_parameter_name = get_kernel_parameter_name (parameter);

    current = g_stpcpy (code_fmt, "\t/* Tomographic rotation angle %02d */\n");

    if (vectorized) {
        if (is_parameter_positional (parameter)) {
            /* If the parameter is positional, first load the global 3-tuple to
             * which it belongs (e.g. if it is source_position_x we need to load the
             * source_position) and change only the part specified by parameter.
             * This way the two components can still be governed by the tomographic
             * angle and the third can be optimized for. */
            tmp = g_strconcat ("\t", kernel_parameter_name, " = ", kernel_parameter_name, "_global", "[%d];\n", NULL);
            parameter_assignment = make_parameter_assignment (parameter);
            current = g_stpcpy (current, tmp);
            current = g_stpcpy (current, parameter_assignment);
            g_free (parameter_assignment);
            g_free (tmp);
        }
        /* For vectorized kernel all static transformations become non-static */
        if ((pretransformation = make_static_transformations (vectorized, with_volume, perpendicular_detector,
                                                             shifted_detector, shifted_source, parallel_beam)) == NULL) {
            g_warning ("Error making static transformations");
            return NULL;
        }
        current = g_stpcpy (current, pretransformation);
        g_free (pretransformation);
    }

    current = g_stpcpy (current, vectorized ? "\tvoxel = rotate_z (tomo_%02d, voxel);\n" :
                        "\tvoxel = rotate_z (tomo_%02d, voxel_0);\n");

    if (with_axis) {
        /* Tilted axis of rotation */
        volume_transformation = g_strdup_printf ("\t%s = rotate_y (%s_y%s, %s);\n"
                                                 "\t%s = rotate_x (%s_x%s, %s);\n",
                                                 "voxel", "axis_angle", "[%d]", "voxel",
                                                 "voxel", "axis_angle", "[%d]", "voxel");
        current = g_stpcpy (current, volume_transformation);
        current = g_stpcpy (current, "\n");
        g_free (volume_transformation);
    }
    if (!parallel_beam) {
        /* FDK normalization computation */
        current = g_stpcpy (current, slice_coefficient);
    }

    /* Voxel projection on the detector */
    current = g_stpcpy (current,
                        "\t// Compute the voxel projected on the detector plane in the global coordinates\n"
                        "\t// V = S + u * (V - S)\n");
    current = g_stpcpy (current, make_projection_computation (perpendicular_detector, shifted_detector,
                                                              shifted_source, parallel_beam));

    if (!perpendicular_detector || shifted_detector) {
        /* Transform global coordinates to detector coordinates */
        current = g_stpcpy (current,
                            "\t// Transform the projected coordinates to the detector coordinates, i.e. rotate the\n"
                            "\t// projected voxel to the detector plane\n");
        current = g_stpcpy (current, detector_shift);
        if (!perpendicular_detector) {
            current = g_stpcpy (current, detector_rotation);
        }
    }

    /* Computational data type adjustment */
    pixel_reading = g_strconcat ("\tresult += read_imagef (projection_%02d, sampler, ",
                                 !g_strcmp0 (compute_type, "float") ? "(" : "convert_float2(",
                                 "voxel.xz + center_position[%d].xz)).x", NULL);
    current = g_stpcpy (current, pixel_reading);
    g_free (pixel_reading);

    /* FDK normalization application */
    if (parallel_beam) {
        current = g_stpcpy (current, ";\n\n");
    } else {
        current = g_stpcpy (current, " * coeff * coeff;\n\n");
    }

    if (vectorized) {
        if (parameter != UFO_UNI_RECO_PARAMETER_Z) {
            tmp_2 = g_strconcat (kernel_parameter_name, "\\[%d\\]", NULL);
            tmp = replace_substring (code_fmt, tmp_2, kernel_parameter_name);
            g_free (code_fmt);
            g_free (tmp_2);
            code_fmt = tmp;
        }
    } else {
        tmp = replace_substring (code_fmt, "\\[%d\\]", "");
        g_free (code_fmt);
        code_fmt = tmp;
    }

    current = code;
    for (i = 0; i < burst; i++) {
        /* %02d would result in octa-based indexing which would crash the kernel for burst > 7  */
        str_index = g_strdup_printf ("iteration + %d", i);
        tmp = replace_substring (code_fmt, "%d", str_index);
        g_free (str_index);
        str_index = g_strdup_printf ("%02d", i);
        str_iteration = replace_substring (tmp, "%02d", str_index);
        g_free (str_index);
        g_free (tmp);
        written += strlen (str_iteration);
        if (written > size) {
            g_free (code_fmt);
            g_free (code);
            g_free (str_iteration);
            return NULL;
        }
        current = g_stpcpy (current, str_iteration);
        g_free (str_iteration);
    }
    g_free (code_fmt);
    g_free (kernel_parameter_name);

    return code;
}

/**
 * make_kernel:
 * @template (in): kernel template string
 * @vectorized (in): geometry parameters are vectors
 * @burst (in): how many projections to process in one kernel invocation
 * @with_axis (in): rotate the rotation axis
 * @with_volume: (in): rotate reconstructed volume
 * @perpendicular_detector: (in): is the detector perpendicular to the beam
 * @shifted_detector: (in): is the detector laterally shifted
 * @shifted_source: (in): is the source laterally shifted
 * @parallel_beam: (in): is the beam parallel
 * @compute_type (in): data type for calculations (one of "half", "float", "double")
 * @result_type (in): data type for storing the intermediate result (one of "half", "float", "double")
 * @store_type (in): data type of the output volume (one of "half", "float", "double",
 * "uchar", "ushort", "uint")
 * @parameter: (in): parameter which represents the third reconstruction axis
 *
 * Make backprojection kernel.
 */
static gchar *
make_kernel (gchar *template, gboolean vectorized, gint burst, gboolean with_axis, gboolean with_volume,
             gboolean perpendicular_detector, gboolean shifted_detector, gboolean shifted_source,
             gboolean parallel_beam, const gchar *compute_type, const gchar *result_type,
             const gchar *store_type, UfoUniRecoParameter parameter)
{
    const gchar *double_pragma_def, *double_pragma, *half_pragma_def, *half_pragma,
          *image_args_fmt, *trigonomoerty_args_fmt;
    gchar *image_args, *trigonometry_args, *type_conversion, *parameter_assignment, *local_assignment,
          *static_transformations, *transformations, *code_tmp, *code, *tmp, **parts;
    gboolean positional_param = is_parameter_positional (parameter);

    double_pragma_def = "#pragma OPENCL EXTENSION cl_khr_fp64 : enable\n";
    half_pragma_def = "#pragma OPENCL EXTENSION cl_khr_fp16 : enable\n\n";
    image_args_fmt = "\t\t\t read_only image2d_t projection_%02d,\n";
    trigonomoerty_args_fmt = "\t\t\t const cfloat2 tomo_%02d,\n";
    parts = g_strsplit (template, "%tmpl%", 9);

    if ((image_args = make_args (burst, image_args_fmt)) == NULL) {
        g_warning ("Error making image arguments");
        return NULL;
    }
    if ((trigonometry_args = make_args (burst, trigonomoerty_args_fmt)) == NULL) {
        g_warning ("Error making trigonometric arguments");
        return NULL;
    }
    if ((type_conversion = make_type_conversion (compute_type, store_type)) == NULL) {
        g_warning ("Error making type conversion");
        return NULL;
    }
    if (vectorized) {
        /* First make a private variable with the same name as the kernel
         * argument name in case of scalar kernel. The buffer value will be
         * loaded for each tomographic angle. */
        local_assignment = make_parameter_initial_assignment (parameter);
        if (local_assignment == NULL) {
            g_warning ("Wrong parameter name");
            return NULL;
        }
        if (positional_param) {
            /* In case of positional parameter two components can still be
             * controlled by the tomographic angle and one can be the actual
             * parameter specified by region, thus the variable needs to be set
             * for every tomographic angle separately */
            parameter_assignment = g_strdup ("");
        }
    }
    if (!vectorized || (vectorized && !positional_param)) {
        parameter_assignment = make_parameter_assignment (parameter);
        if (parameter_assignment == NULL) {
            g_warning ("Wrong parameter name");
            return NULL;
        }
    }

    if (!vectorized) {
        if ((static_transformations = make_static_transformations(FALSE, with_volume, perpendicular_detector,
                                                                  shifted_detector, shifted_source, parallel_beam)) == NULL) {
            g_warning ("Error making static transformations");
            return NULL;
        }
        tmp = replace_substring (static_transformations, "\\[%d\\]", "");
        g_free (static_transformations);
        static_transformations = tmp;
    }
    if ((transformations = make_transformations (parameter, vectorized, burst, with_volume, with_axis,
                                                 perpendicular_detector, shifted_detector, shifted_source,
                                                 parallel_beam, compute_type)) == NULL) {
        g_warning ("Error making tomographic-angle-based transformations");
        return NULL;
    }
    if (!(g_strcmp0 (compute_type, "double") && g_strcmp0 (result_type, "double"))) {
        double_pragma = double_pragma_def;
    } else {
        double_pragma = "";
    }
    if (!(g_strcmp0 (compute_type, "half") && g_strcmp0 (result_type, "half"))) {
        half_pragma = half_pragma_def;
    } else {
        half_pragma = "";
    }
    code_tmp = g_strconcat (double_pragma, half_pragma,
                            parts[0], image_args,
                            parts[1], trigonometry_args,
                            parts[2], vectorized ? local_assignment : "",
                            parts[3], parameter_assignment,
                            parts[4], vectorized ? "" : static_transformations,
                            parts[5], transformations,
                            parts[6], type_conversion,
                            parts[7], type_conversion,
                            parts[8], NULL);
    code = replace_substring (code_tmp, "cfloat", compute_type);
    g_free (code_tmp);
    code_tmp = replace_substring (code, "rtype", result_type);
    g_free (code);
    code = replace_substring (code_tmp, "stype", store_type);

    g_free (image_args);
    g_free (trigonometry_args);
    g_free (type_conversion);
    g_free (parameter_assignment);
    if (vectorized) {
        g_free (local_assignment);
    } else {
        g_free (static_transformations);
    }
    g_free (transformations);
    g_free (code_tmp);
    g_strfreev (parts);

    return code;
}
/*}}}*/

/*{{{ OpenCL helper functions */
DEFINE_CREATE_REGIONS (cl_float)
DEFINE_CREATE_REGIONS (cl_double)

static void
create_images (UfoGeneralBackprojectTaskPrivate *priv, gsize width, gsize height)
{
    cl_image_format image_fmt;
    cl_int cl_error;
    guint i;

    g_log ("gbp", G_LOG_LEVEL_DEBUG, "Creating images %lu x %lu", width, height);

    /* TODO: dangerous, don't rely on the ufo-buffer */
    image_fmt.image_channel_order = CL_INTENSITY;
    image_fmt.image_channel_data_type = CL_FLOAT;

    for (i = 0; i < priv->burst; i++) {
        /* TODO: what about the "other" API? */
        priv->projections[i] = clCreateImage2D (priv->context,
                                                CL_MEM_READ_ONLY,
                                                &image_fmt,
                                                width,
                                                height,
                                                0,
                                                NULL,
                                                &cl_error);
        UFO_RESOURCES_CHECK_CLERR (cl_error);
    }
}

static cl_mem
transfer_host_to_device (cl_context context, gpointer host_array, gsize num_bytes)
{
    cl_int cl_error;
    cl_mem device_array;

    device_array = clCreateBuffer (context,
                                   CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                   num_bytes,
                                   host_array,
                                   &cl_error);
    UFO_RESOURCES_CHECK_CLERR (cl_error);

    return device_array;
}

DEFINE_TRANSFER_ANGULAR_ARGUMENT (cl_float)
DEFINE_TRANSFER_ANGULAR_ARGUMENT (cl_double)
DEFINE_TRANSFER_POSITINAL_ARGUMENT (cl_float)
DEFINE_TRANSFER_POSITINAL_ARGUMENT (cl_double)
DEFINE_SET_ANGULAR_VECTOR_KERNEL_ARGUMENT (cl_float)
DEFINE_SET_ANGULAR_VECTOR_KERNEL_ARGUMENT (cl_double)
DEFINE_SET_STATIC_SCALAR_ARGUMENTS (cl_float)
DEFINE_SET_STATIC_SCALAR_ARGUMENTS (cl_double)
DEFINE_SET_STATIC_VECTOR_ARGUMENTS (cl_float)
DEFINE_SET_STATIC_VECTOR_ARGUMENTS (cl_double)
DEFINE_COMPUTE_SLICE_REGION (cl_float)
DEFINE_COMPUTE_SLICE_REGION (cl_double)
DEFINE_SET_STATIC_ARGS (cl_float)
DEFINE_SET_STATIC_ARGS (cl_double)

/**
 * Copy buffer to OpenCL image.
 */
static void
copy_to_image (const cl_command_queue cmd_queue,
               UfoBuffer *input,
               cl_mem output,
               gsize width,
               gsize height)
{
    cl_event event;
    cl_int errcode;
    cl_mem input_array;
    const size_t origin[] = {0, 0, 0};
    const size_t region[] = {width, height, 1};

    input_array = ufo_buffer_get_device_array (input, cmd_queue);
    errcode = clEnqueueCopyBufferToImage (cmd_queue,
                                          input_array,
                                          output,
                                          0, origin, region,
                                          0, NULL, &event);

    UFO_RESOURCES_CHECK_CLERR (errcode);
    UFO_RESOURCES_CHECK_CLERR (clWaitForEvents (1, &event));
    UFO_RESOURCES_CHECK_CLERR (clReleaseEvent (event));
}

static void
node_setup (UfoGeneralBackprojectTaskPrivate *priv,
            UfoGpuNode *node)
{
    guint i;
    gboolean with_axis, with_volume, parallel_beam, perpendicular_detector, shifted_detector, shifted_source;
    gchar *template, *kernel_code, *compiler_options = NULL;
    const gchar *node_name;
    GValue *node_name_gvalue;
    const gchar compiler_options_tmpl[] = "-cl-nv-maxrregcount=%u";
    UfoUniRecoNodeProps *node_props;

    /* GPU type specific settings */
    node_name_gvalue = ufo_gpu_node_get_info (node, UFO_GPU_NODE_INFO_NAME);
    node_name = g_value_get_string (node_name_gvalue);
    if (!(node_props = g_hash_table_lookup (priv->node_props_table, node_name))) {
        g_log ("gbp", G_LOG_LEVEL_DEBUG, "GPU with name %s not in database", node_name);
        node_props = g_hash_table_lookup (priv->node_props_table, "GENERIC");
    }
    if (!priv->burst) {
        priv->burst = node_props->burst;
    }
    if (node_props->max_regcount) {
        compiler_options = g_strdup_printf (compiler_options_tmpl, node_props->max_regcount);
    }
    g_log ("gbp", G_LOG_LEVEL_DEBUG,
           "GPU node %s properties: burst: %u, compiler options: '%s'",
           node_name, priv->burst, compiler_options);
    g_value_unset (node_name_gvalue);

    /* Initialization */
    /* Assume the most efficient geometry, change if necessary */
    with_axis = is_axis_parameter (priv->parameter) ||
                !(ufo_scarray_is_almost_zero (priv->geometry->axis->angle->x) &&
                  ufo_scarray_is_almost_zero (priv->geometry->axis->angle->y));
    with_volume = is_volume_parameter (priv->parameter) || !ufo_scpoint_are_almost_zero (priv->geometry->volume_angle);
    shifted_detector = !(ufo_scarray_is_almost_zero (priv->geometry->detector->position->x) &&
                         ufo_scarray_is_almost_zero (priv->geometry->detector->position->z));
    shifted_source = !(ufo_scarray_is_almost_zero (priv->geometry->source_position->x) &&
                         ufo_scarray_is_almost_zero (priv->geometry->source_position->z));
    perpendicular_detector = !is_detector_rotation_parameter (priv->parameter) &&
                             !is_detector_position_parameter (priv->parameter) &&
                             ufo_scpoint_are_almost_zero (priv->geometry->detector->angle);
    parallel_beam = TRUE;
    /* Actual parameter setup */
    for (i = 0; i < priv->num_projections; i++) {
        parallel_beam = parallel_beam && isinf (ufo_scarray_get_double (priv->geometry->source_position->y, i));
    }
    priv->vectorized = (ufo_scarray_has_n_values (priv->geometry->axis->angle->x, priv->num_projections) ||
                        ufo_scarray_has_n_values (priv->geometry->axis->angle->y, priv->num_projections) ||
                        ufo_scarray_has_n_values (priv->geometry->volume_angle->x, priv->num_projections) ||
                        ufo_scarray_has_n_values (priv->geometry->volume_angle->y, priv->num_projections) ||
                        ufo_scarray_has_n_values (priv->geometry->volume_angle->z, priv->num_projections) ||
                        ufo_scarray_has_n_values (priv->geometry->detector->angle->x, priv->num_projections) ||
                        ufo_scarray_has_n_values (priv->geometry->detector->angle->y, priv->num_projections) ||
                        ufo_scarray_has_n_values (priv->geometry->detector->angle->z, priv->num_projections) ||
                        ufo_scarray_has_n_values (priv->geometry->detector->position->x, priv->num_projections) ||
                        ufo_scarray_has_n_values (priv->geometry->detector->position->y, priv->num_projections) ||
                        ufo_scarray_has_n_values (priv->geometry->detector->position->z, priv->num_projections) ||
                        ufo_scarray_has_n_values (priv->geometry->source_position->x, priv->num_projections) ||
                        ufo_scarray_has_n_values (priv->geometry->source_position->y, priv->num_projections) ||
                        ufo_scarray_has_n_values (priv->geometry->source_position->z, priv->num_projections) ||
                        ufo_scarray_has_n_values (priv->geometry->axis->position->x, priv->num_projections) ||
                        ufo_scarray_has_n_values (priv->geometry->axis->position->z, priv->num_projections));

    g_log ("gbp", G_LOG_LEVEL_DEBUG, "vectorized: %d, parameter: %s with axis: %d, with volume: %d, "
           "perpendicular detector: %d, parallel beam: %d, "
           "compute type: %s, result type: %s, store type: %s",
             priv->vectorized, parameter_values[priv->parameter].value_nick, with_axis, with_volume,
             perpendicular_detector, parallel_beam,
             compute_type_values[priv->compute_type].value_nick,
             ft_values[priv->result_type].value_nick,
             st_values[priv->store_type].value_nick);

    if ((template = make_template (priv)) == NULL) {
        return;
    }

    /* Create kernel source code based on geometry settings */
    kernel_code = make_kernel (template, priv->vectorized, priv->burst, with_axis, with_volume,
                               perpendicular_detector, shifted_detector, shifted_source,
                               parallel_beam, compute_type_values[priv->compute_type].value_nick,
                               ft_values[priv->result_type].value_nick,
                               st_values[priv->store_type].value_nick,
                               priv->parameter);
    if (!kernel_code) {
        g_free (template);
        g_free (compiler_options);
        return;
    }
    priv->kernel = ufo_resources_get_kernel_from_source (priv->resources,
                                                         kernel_code,
                                                         "backproject",
                                                         compiler_options,
                                                         NULL);
    g_free (kernel_code);

    if (priv->num_projections % priv->burst) {
        kernel_code = make_kernel (template, priv->vectorized, priv->num_projections % priv->burst,
                                   with_axis, with_volume, perpendicular_detector,
                                   shifted_detector, shifted_source, parallel_beam,
                                   compute_type_values[priv->compute_type].value_nick,
                                   ft_values[priv->result_type].value_nick,
                                   st_values[priv->store_type].value_nick,
                                   priv->parameter);
        if (!kernel_code) {
            g_free (template);
            g_free (compiler_options);
            return;
        }

        /* If num_projections % priv->burst != 0 we need one more kernel to process the remaining projections */
        priv->rest_kernel = ufo_resources_get_kernel_from_source (priv->resources,
                                                                  kernel_code,
                                                                  "backproject",
                                                                  compiler_options,
                                                                  NULL);
        /* g_printf ("%s", kernel_code); */
        g_free (kernel_code);
    }
    g_free (template);
    g_free (compiler_options);

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clRetainKernel (priv->kernel));
    }
    if (priv->rest_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clRetainKernel (priv->rest_kernel));
    }
}
/*}}}*/

UfoNode *
ufo_general_backproject_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_GENERAL_BACKPROJECT_TASK, NULL));
}

static void
ufo_general_backproject_task_setup (UfoTask *task,
                                    UfoResources *resources,
                                    GError **error)
{
    cl_int cl_error;
    guint i;
    GValue tomo_angle = G_VALUE_INIT;
    UfoGeneralBackprojectTaskPrivate *priv = UFO_GENERAL_BACKPROJECT_TASK_GET_PRIVATE (task);
    priv->resources = g_object_ref (resources);
    priv->kernel = NULL;
    priv->rest_kernel = NULL;
    priv->projections = NULL;
    priv->chunks = NULL;
    priv->cl_regions = NULL;
    priv->vector_arguments = NULL;

    /* Check parameter values */
    if (!priv->num_projections) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "Number of projections not set");
        return;
    }

    if (!ufo_scarray_has_n_values (priv->geometry->axis->angle->z, priv->num_projections)) {
        /* Create equidistant tomographic angles and treat the one specified in
         * the current priv->geometry->axis->angle->z as offset */
        ufo_scarray_free (priv->geometry->axis->angle->z);
        priv->geometry->axis->angle->z = ufo_scarray_new (priv->num_projections, G_TYPE_DOUBLE, NULL);
        g_value_init (&tomo_angle, G_TYPE_DOUBLE);
        for (i = 0; i < priv->num_projections; i++) {
            g_value_set_double (&tomo_angle, ((gdouble) i) / priv->num_projections * priv->overall_angle);
            ufo_scarray_insert (priv->geometry->axis->angle->z, i, &tomo_angle);
        }
        g_value_unset (&tomo_angle);
    }

    if (priv->gray_map_min >= priv->gray_map_max &&
        (priv->store_type == ST_UCHAR || priv->store_type == ST_USHORT || priv->store_type == ST_UINT)) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "Gray mapping minimum must be less then the maximum");
        return;
    }

    priv->node_props_table = ufo_get_node_props_table ();

    /* Set OpenCL variables */
    priv->context = ufo_resources_get_context (resources);
    UFO_RESOURCES_CHECK_CLERR (clRetainContext (priv->context));
    priv->sampler = clCreateSampler (priv->context, (cl_bool) FALSE, priv->addressing_mode, CL_FILTER_LINEAR, &cl_error);
    UFO_RESOURCES_CHECK_CLERR (cl_error);
}

static void
ufo_general_backproject_task_get_requisition (UfoTask *task,
                                              UfoBuffer **inputs,
                                              UfoRequisition *requisition,
                                              GError **error)
{
    UfoGeneralBackprojectTaskPrivate *priv;
    UfoGpuNode *node;
    cl_command_queue cmd_queue;
    UfoRequisition in_req;
    gdouble region_start, region_stop, region_step;
    gsize slice_size, chunk_size, volume_size, projections_size;
    GValue *max_global_mem_size_gvalue, *max_mem_alloc_size_gvalue;
    cl_ulong max_global_mem_size, max_mem_alloc_size;
    cl_int cl_error;
    guint i;
    typedef void (*CreateRegionFunc) (UfoGeneralBackprojectTaskPrivate *, const cl_command_queue,
                                      const gdouble, const gdouble);
    typedef void (*SetStaticArgsFunc) (UfoTask *, UfoRequisition *, const cl_kernel);
    CreateRegionFunc create_regions[2] = {create_regions_cl_float, create_regions_cl_double};
    SetStaticArgsFunc set_static_args[2] = {set_static_args_cl_float, set_static_args_cl_double};
    GValue g_value_int = G_VALUE_INIT;
    g_value_init (&g_value_int, G_TYPE_INT);

    priv = UFO_GENERAL_BACKPROJECT_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    g_assert_true (ufo_scarray_has_n_values (priv->region, 3));
    requisition->n_dims = 2;
    ufo_buffer_get_requisition (inputs[0], &in_req);

    if (ufo_scarray_get_double (priv->region_x, 2) == 0.0) {
        /* If the slice width is not set, reconstruct full width */
        requisition->dims[0] = in_req.dims[0];
    } else {
        requisition->dims[0] = REGION_SIZE (priv->region_x);
    }
    if (ufo_scarray_get_double (priv->region_y, 2) == 0.0) {
        /* If the slice height is not set, reconstruct full height, which is the
         * same as width */
        requisition->dims[1] = in_req.dims[0];
    } else {
        requisition->dims[1] = REGION_SIZE (priv->region_y);
    }

    if (!priv->kernel) {
        /* First iteration, setup kernels */
        node_setup (priv, node);
        if (!priv->kernel) {
            g_set_error_literal (error, UFO_TASK_ERROR, UFO_TASK_ERROR_GET_REQUISITION,
                                 "Error creating backprojection kernels");
            return;
        }

        if (UFO_MATH_ARE_ALMOST_EQUAL (ufo_scarray_get_double (priv->region, 2), 0)) {
            /* Conservative approach, reconstruct just one slice */
            region_start = 0.0f;
            region_stop = 1.0f;
            region_step = 1.0f;
        } else {
            region_start = ufo_scarray_get_double (priv->region, 0);
            region_stop = ufo_scarray_get_double (priv->region, 1);
            region_step = ufo_scarray_get_double (priv->region, 2);
        }
        g_log ("gbp", G_LOG_LEVEL_DEBUG, "region: %g %g %g", region_start, region_stop, region_step);
        priv->num_slices = (gsize) ceil ((region_stop - region_start) / region_step);
        max_global_mem_size_gvalue = ufo_gpu_node_get_info (node, UFO_GPU_NODE_INFO_GLOBAL_MEM_SIZE);
        max_global_mem_size = g_value_get_ulong (max_global_mem_size_gvalue);
        g_value_unset (max_global_mem_size_gvalue);
        projections_size = priv->burst * in_req.dims[0] * in_req.dims[1] * sizeof (cl_float);
        slice_size = requisition->dims[0] * requisition->dims[1] * get_type_size (priv->store_type);
        volume_size = slice_size * priv->num_slices;
        max_mem_alloc_size_gvalue = ufo_gpu_node_get_info (node, UFO_GPU_NODE_INFO_MAX_MEM_ALLOC_SIZE);
        /* Even if a card claims to be able to allocate more than 4 GB (e.g. RTX
         * 8000) we get OpenCL errors, so limit it to 4 GB */
        max_mem_alloc_size = MIN (g_value_get_ulong (max_mem_alloc_size_gvalue), ((cl_ulong) 1) << 32);
        g_value_unset (max_mem_alloc_size_gvalue);
        priv->num_slices_per_chunk = (guint) floor ((gdouble) MIN (max_mem_alloc_size, volume_size) / ((gdouble) slice_size));
        if (projections_size + volume_size > max_global_mem_size) {
            g_set_error_literal (error, UFO_TASK_ERROR, UFO_TASK_ERROR_GET_REQUISITION,
                                 "Volume size doesn't fit to memory");
            return;
        }

        priv->projections = (cl_mem *) g_malloc (priv->burst * sizeof (cl_mem));
        /* Create subvolumes (because one large volume might be larger than the maximum allocatable memory chunk */
        priv->num_chunks = (priv->num_slices - 1) / priv->num_slices_per_chunk + 1;
        chunk_size = priv->num_slices_per_chunk * slice_size;
        g_log ("gbp", G_LOG_LEVEL_DEBUG, "Max alloc size: %lu, max global size: %lu", max_mem_alloc_size, max_global_mem_size);
        g_log ("gbp", G_LOG_LEVEL_DEBUG, "Num chunks: %d, chunk size: %lu, num slices per chunk: %u",
               priv->num_chunks, chunk_size, priv->num_slices_per_chunk);
        g_log ("gbp", G_LOG_LEVEL_DEBUG, "Volume size: %lu, num slices: %u", volume_size, priv->num_slices);
        priv->chunks = (cl_mem *) g_malloc (priv->num_chunks * sizeof (cl_mem));
        if (!priv->chunks) {
            g_set_error_literal (error, UFO_TASK_ERROR, UFO_TASK_ERROR_GET_REQUISITION,
                                 "Error allocating volume chunks");
            return;
        }
        priv->cl_regions = (cl_mem *) g_malloc (priv->num_chunks * sizeof (cl_mem));
        if (!priv->cl_regions) {
            g_set_error_literal (error, UFO_TASK_ERROR, UFO_TASK_ERROR_GET_REQUISITION,
                                 "Error allocating volume chunks");
            return;
        }
        for (i = 0; i < priv->num_chunks; i++) {
            g_log ("gbp", G_LOG_LEVEL_DEBUG, "Creating chunk %d with size %lu",
                   i, MIN (volume_size, (i + 1) * chunk_size) - i * chunk_size);
            priv->chunks[i] = clCreateBuffer (priv->context,
                                              CL_MEM_WRITE_ONLY,
                                              MIN (volume_size, (i + 1) * chunk_size) - i * chunk_size,
                                              NULL,
                                              &cl_error);
            UFO_RESOURCES_CHECK_CLERR (cl_error);
        }
        create_images (priv, in_req.dims[0], in_req.dims[1]);
        create_regions[priv->compute_type] (priv, cmd_queue, region_start, region_step);
        set_static_args[priv->compute_type] (task, requisition, priv->kernel);
        if (priv->rest_kernel) {
            set_static_args[priv->compute_type] (task, requisition, priv->rest_kernel);
        }
    }

    g_log ("gbp", G_LOG_LEVEL_DEBUG, "requisition (x, y, z): %lu %lu %d", requisition->dims[0], requisition->dims[1], 1);
}

static guint
ufo_general_backproject_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_general_backproject_task_get_num_dimensions (UfoTask *task,
                                             guint input)
{
    g_return_val_if_fail (input == 0, 0);

    return 2;
}

static UfoTaskMode
ufo_general_backproject_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_REDUCTOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_general_backproject_task_process (UfoTask *task,
                                      UfoBuffer **inputs,
                                      UfoBuffer *output,
                                      UfoRequisition *requisition)
{
    UfoGeneralBackprojectTaskPrivate *priv;
    UfoRequisition in_req;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    guint i, index, ki;
    guint count, burst, num_slices_current_chunk;
    cl_kernel kernel;
    cl_command_queue cmd_queue;
    gdouble rot_angle;
    cl_float f_tomo_angle[2];
    cl_double d_tomo_angle[2];
    cl_int iteration;
    gsize local_work_size[3] = {1, 1, 1};
    gsize global_work_size[3];
    gint real_size[4];
    GValue *max_work_group_size_gval;
    gulong max_work_group_size;

    priv = UFO_GENERAL_BACKPROJECT_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    ufo_buffer_get_requisition (inputs[0], &in_req);
    g_object_get (task, "num_processed", &count, NULL);

    if (count >= priv->num_projections / priv->burst * priv->burst) {
        kernel = priv->rest_kernel;
        burst = priv->num_projections % priv->burst;
        index = (count - priv->num_projections / priv->burst * priv->burst) % burst;
    } else {
        kernel = priv->kernel;
        burst = priv->burst;
        index = count % burst;
    }

    /* Local work size determined by the maximum supported work group size */
    max_work_group_size_gval = ufo_gpu_node_get_info (node, UFO_GPU_NODE_INFO_MAX_WORK_GROUP_SIZE);
    max_work_group_size = g_value_get_ulong (max_work_group_size_gval);
    g_value_unset (max_work_group_size_gval);
    for (i = 0; i < (guint) log2 (max_work_group_size); i++) {
        local_work_size[i % 3] *= 2;
    }

    global_work_size[0] = requisition->dims[0] % local_work_size[0] ?
                          NEXT_DIVISOR (requisition->dims[0], local_work_size[0]) :
                          requisition->dims[0];
    global_work_size[1] = requisition->dims[1] % local_work_size[1] ?
                          NEXT_DIVISOR (requisition->dims[1], local_work_size[1]) :
                          requisition->dims[1];
    global_work_size[2] = priv->num_slices_per_chunk % local_work_size[2] ?
                          NEXT_DIVISOR (priv->num_slices_per_chunk, local_work_size[2]) :
                          priv->num_slices_per_chunk;
    real_size[0] = requisition->dims[0];
    real_size[1] = requisition->dims[1];
    real_size[3] = 0;

    if (!count) {
        g_log ("gbp", G_LOG_LEVEL_DEBUG, "Global work size: %lu %lu %lu, local: %lu %lu %lu",
               global_work_size[0], global_work_size[1], global_work_size[2],
               local_work_size[0], local_work_size[1], local_work_size[2]);
    }

    /* Setup tomographic rotation angle dependent arguments */
    ki = STATIC_ARG_OFFSET + burst;
    rot_angle = ufo_scarray_get_double (priv->geometry->axis->angle->z, count);
    if (priv->compute_type == CT_FLOAT) {
        fill_sincos_cl_float (f_tomo_angle, rot_angle);
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, ki + index, sizeof (cl_float2), f_tomo_angle));
    } else {
        fill_sincos_cl_double (d_tomo_angle, rot_angle);
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, ki + index, sizeof (cl_double2), d_tomo_angle));
    }
    copy_to_image (cmd_queue, inputs[0], priv->projections[index], in_req.dims[0], in_req.dims[1]);

    if (index + 1 == burst) {
        profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
        ki += index + 1;
        iteration = (cl_int) (count + 1 - burst);
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, ki++, sizeof (cl_int), &iteration));
        for (i = 0; i < priv->num_chunks; i++) {
            /* The last chunk might be smaller */
            num_slices_current_chunk = MIN (priv->num_slices,  (i + 1) * priv->num_slices_per_chunk) - i * priv->num_slices_per_chunk;
            real_size[2] = (gint) num_slices_current_chunk;
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, REAL_SIZE_ARG_INDEX, sizeof (cl_int3), real_size));
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, ki, sizeof (cl_mem), &priv->chunks[i]));
            UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (kernel, ki + 1, sizeof (cl_mem), &priv->cl_regions[i]));
            ufo_profiler_call_blocking (profiler, cmd_queue, kernel, 3, global_work_size, local_work_size);
        }
    }

    return TRUE;
}

static gboolean
ufo_general_backproject_task_generate (UfoTask *task,
                                       UfoBuffer *output,
                                       UfoRequisition *requisition)
{
    UfoGeneralBackprojectTaskPrivate *priv;
    UfoGpuNode *node;
    cl_command_queue cmd_queue;
    cl_mem out_mem;
    guint count, chunk_index;
    /* TODO: handle other data types */
    size_t bpp;
    size_t src_row_pitch, src_slice_pitch;
    size_t src_origin[3] = {0, 0, 0};
    size_t dst_origin[3] = {0, 0, 0};
    size_t region[3] = {0, 0, 1};

    priv = UFO_GENERAL_BACKPROJECT_TASK_GET_PRIVATE (task);
    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);
    chunk_index = priv->generated / priv->num_slices_per_chunk;
    bpp = get_type_size (priv->store_type);
    g_object_get (task, "num_processed", &count, NULL);

    if (count != priv->num_projections) {
        /* Don't send volume if not enough projections came */
        g_warning ("general-backproject received only %u projections out of %u "
                   "specified, no outuput will be generated", count, priv->num_projections);
        return FALSE;
    }
    if (priv->generated >= priv->num_slices) {
        /* Don't send volume if not enough projections came */
        return FALSE;
    }

    src_row_pitch = requisition->dims[0] * bpp;
    src_slice_pitch = src_row_pitch * requisition->dims[1];
    src_origin[2] = priv->generated % priv->num_slices_per_chunk;
    region[0] = src_row_pitch;
    region[1] = requisition->dims[1];
    g_log ("gbp", G_LOG_LEVEL_DEBUG, "Generating slice %u from chunk %u", priv->generated + 1, chunk_index);
    g_log ("gbp", G_LOG_LEVEL_DEBUG, "src_origin: %lu %lu %lu", src_origin[0], src_origin[1], src_origin[2]);
    g_log ("gbp", G_LOG_LEVEL_DEBUG, "region: %lu %lu %lu", region[0], region[1], region[2]);
    g_log ("gbp", G_LOG_LEVEL_DEBUG, "row pitch %lu, slice pitch %lu", src_row_pitch, src_slice_pitch);

    UFO_RESOURCES_CHECK_CLERR (clEnqueueCopyBufferRect (cmd_queue,
                                                        priv->chunks[chunk_index], out_mem,
                                                        src_origin, dst_origin, region,
                                                        src_row_pitch, src_slice_pitch,
                                                        src_row_pitch, 0,
                                                        0, NULL, NULL));

    priv->generated++;

    return TRUE;
}

/*{{{ Setters and getters and properties initialization */
static void
ufo_general_backproject_task_set_property (GObject *object,
                                           guint property_id,
                                           const GValue *value,
                                           GParamSpec *pspec)
{
    UfoGeneralBackprojectTaskPrivate *priv = UFO_GENERAL_BACKPROJECT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_BURST:
            priv->burst = g_value_get_uint (value);
            break;
        case PROP_PARAMETER:
            priv->parameter = g_value_get_enum (value);
            break;
        case PROP_Z:
            priv->z = g_value_get_double (value);
            break;
        case PROP_REGION:
            ufo_scarray_get_value (priv->region, value);
            break;
        case PROP_REGION_X:
            ufo_scarray_get_value (priv->region_x, value);
            break;
        case PROP_REGION_Y:
            ufo_scarray_get_value (priv->region_y, value);
            break;
        case PROP_CENTER_POSITION_X:
            ufo_scarray_get_value (priv->geometry->axis->position->x, value);
            break;
        case PROP_CENTER_POSITION_Z:
            ufo_scarray_get_value (priv->geometry->axis->position->z, value);
            break;
        case PROP_SOURCE_POSITION_X:
            ufo_scarray_get_value (priv->geometry->source_position->x, value);
            break;
        case PROP_SOURCE_POSITION_Y:
            ufo_scarray_get_value (priv->geometry->source_position->y, value);
            break;
        case PROP_SOURCE_POSITION_Z:
            ufo_scarray_get_value (priv->geometry->source_position->z, value);
            break;
        case PROP_DETECTOR_POSITION_X:
            ufo_scarray_get_value (priv->geometry->detector->position->x, value);
            break;
        case PROP_DETECTOR_POSITION_Y:
            ufo_scarray_get_value (priv->geometry->detector->position->y, value);
            break;
        case PROP_DETECTOR_POSITION_Z:
            ufo_scarray_get_value (priv->geometry->detector->position->z, value);
            break;
        case PROP_DETECTOR_ANGLE_X:
            ufo_scarray_get_value (priv->geometry->detector->angle->x, value);
            break;
        case PROP_DETECTOR_ANGLE_Y:
            ufo_scarray_get_value (priv->geometry->detector->angle->y, value);
            break;
        case PROP_DETECTOR_ANGLE_Z:
            ufo_scarray_get_value (priv->geometry->detector->angle->z, value);
            break;
        case PROP_AXIS_ANGLE_X:
            ufo_scarray_get_value (priv->geometry->axis->angle->x, value);
            break;
        case PROP_AXIS_ANGLE_Y:
            ufo_scarray_get_value (priv->geometry->axis->angle->y, value);
            break;
        case PROP_AXIS_ANGLE_Z:
            ufo_scarray_get_value (priv->geometry->axis->angle->z, value);
            break;
        case PROP_VOLUME_ANGLE_X:
            ufo_scarray_get_value (priv->geometry->volume_angle->x, value);
            break;
        case PROP_VOLUME_ANGLE_Y:
            ufo_scarray_get_value (priv->geometry->volume_angle->y, value);
            break;
        case PROP_VOLUME_ANGLE_Z:
            ufo_scarray_get_value (priv->geometry->volume_angle->z, value);
            break;
        case PROP_NUM_PROJECTIONS:
            priv->num_projections = g_value_get_uint (value);
            break;
        case PROP_COMPUTE_TYPE:
            priv->compute_type = g_value_get_enum (value);
            break;
        case PROP_RESULT_TYPE:
            priv->result_type = g_value_get_enum (value);
            break;
        case PROP_STORE_TYPE:
            priv->store_type = g_value_get_enum (value);
            break;
        case PROP_OVERALL_ANGLE:
            priv->overall_angle = g_value_get_double (value);
            break;
        case PROP_ADDRESSING_MODE:
            priv->addressing_mode = g_value_get_enum (value);
            break;
        case PROP_GRAY_MAP_MIN:
            priv->gray_map_min = g_value_get_double (value);
            break;
        case PROP_GRAY_MAP_MAX:
            priv->gray_map_max = g_value_get_double (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_general_backproject_task_get_property (GObject *object,
                                           guint property_id,
                                           GValue *value,
                                           GParamSpec *pspec)
{
    UfoGeneralBackprojectTaskPrivate *priv = UFO_GENERAL_BACKPROJECT_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_BURST:
            g_value_set_uint (value, priv->burst);
            break;
        case PROP_PARAMETER:
            g_value_set_enum (value, priv->parameter);
            break;
        case PROP_Z:
            g_value_set_double (value, priv->z);
            break;
        case PROP_REGION:
            ufo_scarray_set_value (priv->region, value);
            break;
        case PROP_REGION_X:
            ufo_scarray_set_value (priv->region_x, value);
            break;
        case PROP_REGION_Y:
            ufo_scarray_set_value (priv->region_y, value);
            break;
        case PROP_CENTER_POSITION_X:
            ufo_scarray_set_value (priv->geometry->axis->position->x, value);
            break;
        case PROP_CENTER_POSITION_Z:
            ufo_scarray_set_value (priv->geometry->axis->position->z, value);
            break;
        case PROP_SOURCE_POSITION_X:
            ufo_scarray_set_value (priv->geometry->source_position->x, value);
            break;
        case PROP_SOURCE_POSITION_Y:
            ufo_scarray_set_value (priv->geometry->source_position->y, value);
            break;
        case PROP_SOURCE_POSITION_Z:
            ufo_scarray_set_value (priv->geometry->source_position->z, value);
            break;
        case PROP_DETECTOR_POSITION_X:
            ufo_scarray_set_value (priv->geometry->detector->position->x, value);
            break;
        case PROP_DETECTOR_POSITION_Y:
            ufo_scarray_set_value (priv->geometry->detector->position->y, value);
            break;
        case PROP_DETECTOR_POSITION_Z:
            ufo_scarray_set_value (priv->geometry->detector->position->z, value);
            break;
        case PROP_DETECTOR_ANGLE_X:
            ufo_scarray_set_value (priv->geometry->detector->angle->x, value);
            break;
        case PROP_DETECTOR_ANGLE_Y:
            ufo_scarray_set_value (priv->geometry->detector->angle->y, value);
            break;
        case PROP_DETECTOR_ANGLE_Z:
            ufo_scarray_set_value (priv->geometry->detector->angle->z, value);
            break;
        case PROP_AXIS_ANGLE_X:
            ufo_scarray_set_value (priv->geometry->axis->angle->x, value);
            break;
        case PROP_AXIS_ANGLE_Y:
            ufo_scarray_set_value (priv->geometry->axis->angle->y, value);
            break;
        case PROP_AXIS_ANGLE_Z:
            ufo_scarray_set_value (priv->geometry->axis->angle->z, value);
            break;
        case PROP_VOLUME_ANGLE_X:
            ufo_scarray_set_value (priv->geometry->volume_angle->x, value);
            break;
        case PROP_VOLUME_ANGLE_Y:
            ufo_scarray_set_value (priv->geometry->volume_angle->y, value);
            break;
        case PROP_VOLUME_ANGLE_Z:
            ufo_scarray_set_value (priv->geometry->volume_angle->z, value);
            break;
        case PROP_NUM_PROJECTIONS:
            g_value_set_uint (value, priv->num_projections);
            break;
        case PROP_COMPUTE_TYPE:
            g_value_set_enum (value, priv->compute_type);
            break;
        case PROP_RESULT_TYPE:
            g_value_set_enum (value, priv->result_type);
            break;
        case PROP_STORE_TYPE:
            g_value_set_enum (value, priv->store_type);
            break;
        case PROP_OVERALL_ANGLE:
            g_value_set_double (value, priv->overall_angle);
            break;
        case PROP_GRAY_MAP_MIN:
            g_value_set_double (value, priv->gray_map_min);
            break;
        case PROP_GRAY_MAP_MAX:
            g_value_set_double (value, priv->gray_map_max);
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
ufo_general_backproject_task_finalize (GObject *object)
{
    guint i;
    UfoGeneralBackprojectTaskPrivate *priv = UFO_GENERAL_BACKPROJECT_TASK_GET_PRIVATE (object);
    if (priv->resources) {
        g_object_unref (priv->resources);
        priv->resources = NULL;
    }

    ufo_scarray_free (priv->region);
    ufo_scarray_free (priv->region_x);
    ufo_scarray_free (priv->region_y);
    ufo_ctgeometry_free (priv->geometry);
    if (priv->node_props_table) {
        g_hash_table_destroy (priv->node_props_table);
    }

    if (priv->projections) {
        for (i = 0; i < priv->burst; i++) {
            if (priv->projections[i] != NULL) {
                UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->projections[i]));
                priv->projections[i] = NULL;
            }
        }
        g_free (priv->projections);
        priv->projections = NULL;
    }


    if (priv->chunks) {
        for (i = 0; i < priv->num_chunks; i++) {
            UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->chunks[i]));
        }
        g_free (priv->chunks);
        priv->chunks = NULL;
    }

    if (priv->cl_regions) {
        for (i = 0; i < priv->num_chunks; i++) {
            UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->cl_regions[i]));
        }
        g_free (priv->cl_regions);
        priv->cl_regions = NULL;
    }

    if (priv->vector_arguments) {
        for (i = 0; i < NUM_VECTOR_ARGUMENTS; i++) {
            UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->vector_arguments[i]));
        }
        g_free (priv->vector_arguments);
        priv->vector_arguments = NULL;
    }

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }
    if (priv->rest_kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->rest_kernel));
        priv->rest_kernel = NULL;
    }

    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }
    if (priv->sampler) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseSampler (priv->sampler));
        priv->sampler = NULL;
    }

    G_OBJECT_CLASS (ufo_general_backproject_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_general_backproject_task_setup;
    iface->get_num_inputs = ufo_general_backproject_task_get_num_inputs;
    iface->get_num_dimensions = ufo_general_backproject_task_get_num_dimensions;
    iface->get_mode = ufo_general_backproject_task_get_mode;
    iface->get_requisition = ufo_general_backproject_task_get_requisition;
    iface->process = ufo_general_backproject_task_process;
    iface->generate = ufo_general_backproject_task_generate;
}

static void
ufo_general_backproject_task_class_init (UfoGeneralBackprojectTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_general_backproject_task_set_property;
    oclass->get_property = ufo_general_backproject_task_get_property;
    oclass->finalize = ufo_general_backproject_task_finalize;

    GParamSpec *double_region_vals = g_param_spec_double ("double-region-values",
                                                          "Double Region values",
                                                          "Elements in double regions",
                                                          -INFINITY,
                                                          INFINITY,
                                                          0.0,
                                                          G_PARAM_READWRITE);

    properties[PROP_BURST] =
        g_param_spec_uint ("burst",
            "Number of projections processed per one kernel invocation",
            "Number of projections processed per one kernel invocation",
            0, 128, 0,
            G_PARAM_READWRITE);

    properties[PROP_PARAMETER] =
        g_param_spec_enum ("parameter",
            "Which parameter will be varied along the z-axis",
            "Which parameter will be varied along the z-axis",
            g_enum_register_static ("ufo_gbp_parameter", parameter_values),
            UFO_UNI_RECO_PARAMETER_Z,
            G_PARAM_READWRITE);

    properties[PROP_Z] =
        g_param_spec_double ("z",
            "Z coordinate of the reconstructed slice",
            "Z coordinate of the reconstructed slice",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
            G_PARAM_READWRITE);

    properties[PROP_REGION] =
        g_param_spec_value_array ("region",
            "Region for the parameter along z-axis as (from, to, step)",
            "Region for the parameter along z-axis as (from, to, step)",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_REGION_X] =
        g_param_spec_value_array ("x-region",
            "X region for reconstruction (horizontal axis) as (from, to, step)",
            "X region for reconstruction (horizontal axis) as (from, to, step)",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_REGION_Y] =
        g_param_spec_value_array ("y-region",
            "Y region for reconstruction (beam direction axis) as (from, to, step)",
            "Y region for reconstruction (beam direction axis) as (from, to, step)",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_CENTER_POSITION_X] =
        g_param_spec_value_array ("center-position-x",
            "Global x center (horizontal in a projection) of the volume with respect to projections",
            "Global x center (horizontal in a projection) of the volume with respect to projections",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_CENTER_POSITION_Z] =
        g_param_spec_value_array ("center-position-z",
            "Global z center (vertical in a projection) of the volume with respect to projections",
            "Global z center (vertical in a projection) of the volume with respect to projections",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_SOURCE_POSITION_X] =
        g_param_spec_value_array ("source-position-x",
            "X source position (horizontal) in global coordinates [pixels]",
            "X source position (horizontal) in global coordinates [pixels]",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_SOURCE_POSITION_Y] =
        g_param_spec_value_array ("source-position-y",
            "Y source position (beam direction) in global coordinates [pixels]",
            "Y source position (beam direction) in global coordinates [pixels]",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_SOURCE_POSITION_Z] =
        g_param_spec_value_array ("source-position-z",
            "Z source position (vertical) in global coordinates [pixels]",
            "Z source position (vertical) in global coordinates [pixels]",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_DETECTOR_POSITION_X] =
        g_param_spec_value_array ("detector-position-x",
            "X detector position (horizontal) in global coordinates [pixels]",
            "X detector position (horizontal) in global coordinates [pixels]",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_DETECTOR_POSITION_Y] =
        g_param_spec_value_array ("detector-position-y",
            "Y detector position (along beam direction) in global coordinates [pixels]",
            "Y detector position (along beam direction) in global coordinates [pixels]",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_DETECTOR_POSITION_Z] =
        g_param_spec_value_array ("detector-position-z",
            "Z detector position (vertical) in global coordinates [pixels]",
            "Z detector position (vertical) in global coordinates [pixels]",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_DETECTOR_ANGLE_X] =
        g_param_spec_value_array("detector-angle-x",
            "Detector rotation around the x axis [rad] (horizontal)",
            "Detector rotation around the x axis [rad] (horizontal)",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_DETECTOR_ANGLE_Y] =
        g_param_spec_value_array("detector-angle-y",
            "Detector rotation around the y axis [rad] (along beam direction)",
            "Detector rotation around the y axis [rad] (balong eam direction)",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_DETECTOR_ANGLE_Z] =
        g_param_spec_value_array("detector-angle-z",
            "Detector rotation around the z axis [rad] (vertical)",
            "Detector rotation around the z axis [rad] (vertical)",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_AXIS_ANGLE_X] =
        g_param_spec_value_array("axis-angle-x",
            "Rotation axis rotation around the x axis [rad] (laminographic angle, 0 = tomography)",
            "Rotation axis rotation around the x axis [rad] (laminographic angle, 0 = tomography)",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_AXIS_ANGLE_Y] =
        g_param_spec_value_array("axis-angle-y",
            "Rotation axis rotation around the y axis [rad] (along beam direction)",
            "Rotation axis rotation around the y axis [rad] (along beam direction)",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_AXIS_ANGLE_Z] =
        g_param_spec_value_array("axis-angle-z",
            "Rotation axis rotation around the z axis [rad] (vertical)",
            "Rotation axis rotation around the z axis [rad] (vertical)",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_VOLUME_ANGLE_X] =
        g_param_spec_value_array("volume-angle-x",
            "Volume rotation around the x axis [rad] (horizontal)",
            "Volume rotation around the x axis [rad] (horizontal)",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_VOLUME_ANGLE_Y] =
        g_param_spec_value_array("volume-angle-y",
            "Volume rotation around the y axis [rad] (along beam direction)",
            "Volume rotation around the y axis [rad] (along beam direction)",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_VOLUME_ANGLE_Z] =
        g_param_spec_value_array("volume-angle-z",
            "Volume rotation around the z axis [rad] (vertical)",
            "Volume rotation around the z axis [rad] (vertical)",
            double_region_vals,
            G_PARAM_READWRITE);

    properties[PROP_COMPUTE_TYPE] =
        g_param_spec_enum ("compute-type",
            "Data type for performing kernel math operations",
            "Data type for performing kernel math operations "
            "(\"half\", \"float\", \"double\")",
            g_enum_register_static ("ufo_gbp_compute_type", compute_type_values),
            CT_FLOAT,
            G_PARAM_READWRITE);

    properties[PROP_ADDRESSING_MODE] =
        g_param_spec_enum ("addressing-mode",
            "Outlier treatment (\"none\", \"clamp\", \"clamp_to_edge\", \"repeat\")",
            "Outlier treatment (\"none\", \"clamp\", \"clamp_to_edge\", \"repeat\")",
            g_enum_register_static ("ufo_gbp_addressing_mode", addressing_values),
            CL_ADDRESS_CLAMP,
            G_PARAM_READWRITE);

    properties[PROP_RESULT_TYPE] =
        g_param_spec_enum ("result-type",
            "Data type for storing the intermediate gray value for a voxel from various rotation angles",
            "Data type for storing the intermediate gray value for a voxel from various rotation angles "
            "(\"half\", \"float\", \"double\")",
            g_enum_register_static ("ufo_gbp_result_type", ft_values),
            FT_FLOAT,
            G_PARAM_READWRITE);

    properties[PROP_STORE_TYPE] =
        g_param_spec_enum ("store-type",
            "Data type of the output volume",
            "Data type of the output volume "
            "(\"half\", \"float\", \"double\", \"uchar\", \"ushort\", \"uint\")",
            g_enum_register_static ("ufo_gbp_store_type", st_values),
            ST_FLOAT,
            G_PARAM_READWRITE);

    properties[PROP_OVERALL_ANGLE] =
        g_param_spec_double ("overall-angle",
            "Angle covered by all projections [rad]",
            "Angle covered by all projections [rad] (can be negative for negative steps "
            "in case only num-projections is specified",
            -G_MAXDOUBLE, G_MAXDOUBLE, 2 * G_PI,
            G_PARAM_READWRITE);

    properties[PROP_GRAY_MAP_MIN] =
        g_param_spec_double ("gray-map-min",
            "Gray valye which maps to 0 in case of integer store type",
            "Gray valye which maps to 0 in case of integer store type",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0,
            G_PARAM_READWRITE);

    properties[PROP_GRAY_MAP_MAX] =
        g_param_spec_double ("gray-map-max",
            "Gray valye which maps to maximum of the chosen integer type in case of integer store type",
            "Gray valye which maps to maximum of the chosen integer type in case of integer store type",
            -G_MAXDOUBLE, G_MAXDOUBLE, 0,
            G_PARAM_READWRITE);

    properties[PROP_NUM_PROJECTIONS] =
        g_param_spec_uint ("num-projections",
            "Number of projections",
            "Number of projections",
            0, 32768, 0,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoGeneralBackprojectTaskPrivate));
}

static void
ufo_general_backproject_task_init(UfoGeneralBackprojectTask *self)
{
    self->priv = UFO_GENERAL_BACKPROJECT_TASK_GET_PRIVATE(self);
    self->priv->resources = NULL;
    self->priv->node_props_table = NULL;
    self->priv->projections = NULL;
    self->priv->chunks = NULL;
    self->priv->cl_regions = NULL;
    self->priv->vector_arguments = NULL;
    self->priv->context = NULL;
    self->priv->kernel = NULL;
    self->priv->rest_kernel = NULL;
    self->priv->sampler = NULL;

    /* Scalars */
    self->priv->burst = 0;
    self->priv->parameter = UFO_UNI_RECO_PARAMETER_Z;
    self->priv->z = 0.0;
    self->priv->num_projections = 0;
    self->priv->compute_type = CT_FLOAT;
    self->priv->result_type = FT_FLOAT;
    self->priv->store_type = ST_FLOAT;
    self->priv->overall_angle = 2 * G_PI;
    self->priv->addressing_mode = CL_ADDRESS_CLAMP;
    self->priv->gray_map_min = 0.0;
    self->priv->gray_map_max = 0.0;

    /* Value arrays */
    self->priv->region = ufo_scarray_new (3, G_TYPE_DOUBLE, NULL);
    self->priv->region_x = ufo_scarray_new (3, G_TYPE_DOUBLE, NULL);
    self->priv->region_y = ufo_scarray_new (3, G_TYPE_DOUBLE, NULL);
    self->priv->geometry = ufo_ctgeometry_new ();

    /* Private */
    self->priv->vectorized = FALSE;
    self->priv->num_slices = 0;
    self->priv->num_slices_per_chunk = 0;
    self->priv->generated = 0;
}
/*}}}*/
