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

#include <glib.h>
#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "ufo-resources.h"
#include "ufo-gpu-node.h"
#include "ufo-enums.h"
#include "ufo-priv.h"

/**
 * SECTION:ufo-resources
 * @Short_description: Manage OpenCL resources
 * @Title: UfoResources
 *
 * The #UfoResources creates the OpenCL environment and loads OpenCL kernels
 * from disk or directly as a string. By default the kernel search path is in
 * `$datadir/ufo` but can be extended by the `UFO_KERNEL_PATH` environment
 * variable.
 */

static void ufo_resources_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoResources, ufo_resources, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                ufo_resources_initable_iface_init))

#define UFO_RESOURCES_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_RESOURCES, UfoResourcesPrivate))

/**
 * UfoResourcesError:
 * @UFO_RESOURCES_ERROR_GENERAL: General resource problems
 * @UFO_RESOURCES_ERROR_LOAD_PROGRAM: Could not load the OpenCL file
 * @UFO_RESOURCES_ERROR_CREATE_PROGRAM: Could not create a program from
 *      the sources
 * @UFO_RESOURCES_ERROR_BUILD_PROGRAM: Could not build program from
 *      sources
 * @UFO_RESOURCES_ERROR_CREATE_KERNEL: Could not create kernel
 *
 * OpenCL related errors.
 */
GQuark
ufo_resources_error_quark (void)
{
    return g_quark_from_static_string ("ufo-resource-resources-error-quark");
}

struct _UfoResourcesPrivate {
    GError          *construct_error;

    UfoDeviceType    device_type;
    gint             platform_index;

    cl_platform_id   platform;
    cl_context       context;
    cl_channel_order channel_order_2d;  /* Supported channel order for CL_MEM_OBJECT_IMAGE2D type and CL_FLOAT image_channel_data_type */
    cl_channel_order channel_order_3d;  /* Supported channel order for CL_MEM_OBJECT_IMAGE3D type and CL_FLOAT image_channel_data_type */
    cl_uint          n_devices;         /* Number of OpenCL devices per platform id */
    cl_device_id     *devices;          /* Array of OpenCL devices per platform id */
    gchar           **device_names;     /* Array of names for each device */

    GList       *gpu_nodes;

    GList       *paths;         /* List of paths containing kernels and header files */
    GHashTable  *kernel_cache;
    GHashTable  *programs;      /* Maps source to program */
    GList       *kernels;
    GString     *build_opts;
};

enum {
    PROP_0,
    PROP_PLATFORM_INDEX,
    PROP_DEVICE_TYPE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

const gchar *opencl_error_msgs[] = {
    "CL_SUCCESS",
    "CL_DEVICE_NOT_FOUND",
    "CL_DEVICE_NOT_AVAILABLE",
    "CL_COMPILER_NOT_AVAILABLE",
    "CL_MEM_OBJECT_ALLOCATION_FAILURE",
    "CL_OUT_OF_RESOURCES",
    "CL_OUT_OF_HOST_MEMORY",
    "CL_PROFILING_INFO_NOT_AVAILABLE",
    "CL_MEM_COPY_OVERLAP",
    "CL_IMAGE_FORMAT_MISMATCH",
    "CL_IMAGE_FORMAT_NOT_SUPPORTED",
    "CL_BUILD_PROGRAM_FAILURE",
    "CL_MAP_FAILURE",
    "CL_MISALIGNED_SUB_BUFFER_OFFSET",
    "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST",

    /* next IDs start at 30! */
    "CL_INVALID_VALUE",
    "CL_INVALID_DEVICE_TYPE",
    "CL_INVALID_PLATFORM",
    "CL_INVALID_DEVICE",
    "CL_INVALID_CONTEXT",
    "CL_INVALID_QUEUE_PROPERTIES",
    "CL_INVALID_COMMAND_QUEUE",
    "CL_INVALID_HOST_PTR",
    "CL_INVALID_MEM_OBJECT",
    "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR",
    "CL_INVALID_IMAGE_SIZE",
    "CL_INVALID_SAMPLER",
    "CL_INVALID_BINARY",
    "CL_INVALID_BUILD_OPTIONS",
    "CL_INVALID_PROGRAM",
    "CL_INVALID_PROGRAM_EXECUTABLE",
    "CL_INVALID_KERNEL_NAME",
    "CL_INVALID_KERNEL_DEFINITION",
    "CL_INVALID_KERNEL",
    "CL_INVALID_ARG_INDEX",
    "CL_INVALID_ARG_VALUE",
    "CL_INVALID_ARG_SIZE",
    "CL_INVALID_KERNEL_ARGS",
    "CL_INVALID_WORK_DIMENSION",
    "CL_INVALID_WORK_GROUP_SIZE",
    "CL_INVALID_WORK_ITEM_SIZE",
    "CL_INVALID_GLOBAL_OFFSET",
    "CL_INVALID_EVENT_WAIT_LIST",
    "CL_INVALID_EVENT",
    "CL_INVALID_OPERATION",
    "CL_INVALID_GL_OBJECT",
    "CL_INVALID_BUFFER_SIZE",
    "CL_INVALID_MIP_LEVEL",
    "CL_INVALID_GLOBAL_WORK_SIZE"
};

/**
 * ufo_resources_clerr:
 * @error: An OpenCL error code
 *
 * Get a human-readable string representation of @error.
 *
 * Returns: (transfer none): A static string of @error.
 */
const gchar *
ufo_resources_clerr (int error)
{
    static const gchar *invalid = "Invalid error code";
    const gint array_size = sizeof opencl_error_msgs/sizeof(gchar*);
    gint index;

    index = error >= -14 ? -error : -error-15;

    if (index >= 0 && index < array_size)
        return opencl_error_msgs[index];

    return invalid;
}

static gchar *
read_file (const gchar *filename)
{
    gssize length;
    gssize buffer_length;
    FILE *fp = fopen (filename, "r");

    if (fp == NULL)
        return NULL;

    fseek (fp, 0, SEEK_END);
    length = (gsize) ftell (fp);

    if (length < 0)
        return NULL;

    rewind (fp);
    gchar *buffer = (gchar *) g_malloc0 (length + 1);

    if (buffer == NULL) {
        fclose (fp);
        return NULL;
    }

    buffer_length = fread (buffer, 1, length, fp);
    fclose (fp);

    if (buffer_length != length) {
        g_free (buffer);
        return NULL;
    }

    return buffer;
}

static void
release_kernel (cl_kernel kernel)
{
    g_debug ("FREE kernel=%p", (gpointer) kernel);
    UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (kernel));
}

static void
release_program (cl_program program)
{
    g_debug ("FREE program=%p", (gpointer) program);
    UFO_RESOURCES_CHECK_CLERR (clReleaseProgram (program));
}

static gchar *
lookup_kernel_path (UfoResourcesPrivate *priv,
                  const gchar *filename)
{
    GList *it;

    /* Check first if filename is already a path */
    if (g_path_is_absolute (filename)) {
        if (g_file_test (filename, G_FILE_TEST_EXISTS))
            return g_strdup (filename);
        else
            return NULL;
    }

    g_list_for (priv->paths, it) {
        gchar *path;

        path = g_build_filename ((gchar *) it->data, filename, NULL);

        if (g_file_test (path, G_FILE_TEST_EXISTS))
            return path;

        g_free (path);
    }

    return NULL;
}

static gboolean
platform_has_gpus (cl_platform_id platform)
{
    cl_uint n_devices = 0;
    cl_int err;

    err = clGetDeviceIDs (platform,
                          CL_DEVICE_TYPE_GPU,
                          0, NULL, &n_devices);

    if (err != CL_DEVICE_NOT_FOUND)
        UFO_RESOURCES_CHECK_CLERR (err);

    return n_devices > 0;
}

static cl_platform_id
get_preferably_gpu_based_platform (UfoResourcesPrivate *priv)
{
    cl_platform_id *platforms;
    cl_uint n_platforms;
    cl_platform_id candidate = 0;

    UFO_RESOURCES_CHECK_AND_SET (clGetPlatformIDs (0, NULL, &n_platforms), &priv->construct_error);

    if (priv->construct_error != NULL)
        return NULL;

    platforms = g_malloc0 (n_platforms * sizeof (cl_platform_id));
    UFO_RESOURCES_CHECK_AND_SET (clGetPlatformIDs (n_platforms, platforms, NULL), &priv->construct_error);

    g_debug ("INFO found %i OpenCL platforms %i", n_platforms, priv->platform_index);

    /* Check if user set a preferred platform */
    if (priv->platform_index >= 0 && priv->platform_index < (gint) n_platforms) {
        candidate = platforms[priv->platform_index];
        goto platform_found;
    }

    if (n_platforms > 0)
        candidate = platforms[0];

    for (guint i = 0; i < n_platforms; i++) {
        if (platform_has_gpus (platforms[i])) {
            candidate = platforms[i];
            break;
        }
    }

platform_found:
    g_free (platforms);
    return candidate;
}

static gboolean
platform_vendor_has_prefix (cl_platform_id platform,
                            const gchar *prefix)
{
    gboolean has_prefix;
    gchar *str;
    gsize size;

    UFO_RESOURCES_CHECK_CLERR (clGetPlatformInfo (platform, CL_PLATFORM_VENDOR, 0, NULL, &size));
    str = g_malloc0 (size);

    UFO_RESOURCES_CHECK_CLERR (clGetPlatformInfo (platform, CL_PLATFORM_VENDOR, size, str, NULL));
    has_prefix = g_str_has_prefix (str, prefix);

    g_free (str);
    return has_prefix;
}

static void
restrict_to_gpu_subset (UfoResourcesPrivate *priv)
{
    const gchar* var;
    gchar **set;
    guint n_set;
    guint *device_indices;
    guint n_devices;
    cl_device_id *subset;

    var = g_getenv ("UFO_DEVICES");

    if (var == NULL || g_strcmp0 (var, "") == 0)
        return;

    set = g_strsplit (var, ",", 0);
    n_set = g_strv_length (set);
    device_indices = g_new0 (guint, n_set);

    n_devices = 0;

    for (guint i = 0; i < n_set; i++) {
        gchar *endptr;
        guint index;

        index = (guint) g_ascii_strtoull (set[i], &endptr, 0);

        if (endptr != set[i]) {
            if (index < priv->n_devices)
                device_indices[n_devices++] = index;
            else
                g_warning ("Device index %u exceeds number of devices", index);
        }
        else {
            g_warning ("`%s' is not a valid device index", set[i]);
        }
    }

    subset = g_malloc0 (n_devices * sizeof (cl_device_id));

    for (guint i = 0; i < n_devices; i++)
        subset[i] = priv->devices[device_indices[i]];

    /* Not cool but ... who cares */
    g_free (priv->devices);
    priv->devices = subset;
    priv->n_devices = n_devices;

    g_free (device_indices);
    g_strfreev (set);
}

static cl_device_type
get_device_type_from_env (void)
{
    const gchar *var;
    gchar **set;
    guint n_set;
    cl_device_type type = 0;

    var = g_getenv ("UFO_DEVICE_TYPE");

    if (var == NULL || g_strcmp0 (var, "") == 0)
        return 0;

    set = g_strsplit (var, ",", 0);
    n_set = g_strv_length (set);

    for (guint i = 0; i < n_set; i++) {
        if (g_strcmp0 (set[i], "cpu") == 0)
            type |= CL_DEVICE_TYPE_CPU;

        if (g_strcmp0 (set[i], "gpu") == 0)
            type |= CL_DEVICE_TYPE_GPU;

        if (g_strcmp0 (set[i], "acc") == 0)
            type |= CL_DEVICE_TYPE_ACCELERATOR;
    }

    g_strfreev (set);
    return type;
}

static cl_channel_order
get_image_channel_order (cl_context context,
                         cl_mem_object_type image_type)
{
    cl_uint num_image_formats;
    cl_image_format *image_formats;
    cl_channel_order channel_order = 0;

    UFO_RESOURCES_CHECK_CLERR (
            clGetSupportedImageFormats (
                context,
                CL_MEM_READ_WRITE,
                image_type,
                0,
                NULL,
                &num_image_formats
            )
    );

    image_formats = g_malloc0 (num_image_formats * sizeof (cl_image_format));

    UFO_RESOURCES_CHECK_CLERR (
            clGetSupportedImageFormats (
                context,
                CL_MEM_READ_WRITE,
                image_type,
                num_image_formats,
                image_formats,
                NULL
            )
    );

    for (cl_uint i = 0; i < num_image_formats; i++) {
        if (image_formats[i].image_channel_data_type == CL_FLOAT) {
            if (image_formats[i].image_channel_order == CL_R) {
                /* The best one, stop */
                channel_order = CL_R;
                break;
            }
            if (image_formats[i].image_channel_order == CL_LUMINANCE && channel_order != CL_R) {
                /* Second best one, continue searching for CL_R */
                channel_order = CL_LUMINANCE;
            }
            if (image_formats[i].image_channel_order == CL_INTENSITY && channel_order == 0) {
                /* Last resort */
                channel_order = CL_INTENSITY;
            }
        }
    }

    g_free (image_formats);

    return channel_order;
}

static gboolean
initialize_opencl (UfoResourcesPrivate *priv)
{
    cl_device_type device_type;
    cl_int errcode = CL_SUCCESS;

    priv->platform = get_preferably_gpu_based_platform (priv);

    if (priv->platform == NULL)
        return FALSE;

    if (platform_vendor_has_prefix (priv->platform, "NVIDIA"))
        g_string_append (priv->build_opts, "-cl-nv-verbose -DVENDOR_NVIDIA");

    if (platform_vendor_has_prefix (priv->platform, "Advanced Micro Devices"))
        g_string_append (priv->build_opts, "-DVENDOR_AMD");

    device_type = get_device_type_from_env ();

    if (device_type == 0) {
        /*
         * If the user did not set anything from the outside, check the
         * device-type property.
         */
        device_type |= priv->device_type & UFO_DEVICE_CPU ? CL_DEVICE_TYPE_CPU : 0;
        device_type |= priv->device_type & UFO_DEVICE_GPU ? CL_DEVICE_TYPE_GPU : 0;
        device_type |= priv->device_type & UFO_DEVICE_ACC ? CL_DEVICE_TYPE_ACCELERATOR : 0;
    }

    g_debug ("INFO Using CPUs=%c GPUs=%c Accelerators=%c",
             (device_type & CL_DEVICE_TYPE_CPU) != 0 ? 'y' : 'n',
             (device_type & CL_DEVICE_TYPE_GPU) != 0 ? 'y' : 'n',
             (device_type & CL_DEVICE_TYPE_ACCELERATOR) != 0 ? 'y' : 'n');

    errcode = clGetDeviceIDs (priv->platform, device_type, 0, NULL, &priv->n_devices);
    UFO_RESOURCES_CHECK_AND_SET (errcode, &priv->construct_error);

    if (errcode != CL_SUCCESS)
        return FALSE;

    priv->devices = g_malloc0 (priv->n_devices * sizeof (cl_device_id));

    errcode = clGetDeviceIDs (priv->platform, device_type, priv->n_devices, priv->devices, NULL);
    UFO_RESOURCES_CHECK_AND_SET (errcode, &priv->construct_error);

    if (errcode != CL_SUCCESS)
        return FALSE;

    restrict_to_gpu_subset (priv);

    priv->context = clCreateContext (NULL, priv->n_devices, priv->devices, NULL, NULL, &errcode);
    UFO_RESOURCES_CHECK_AND_SET (errcode, &priv->construct_error);

    if (errcode != CL_SUCCESS)
        return FALSE;

    priv->channel_order_2d = get_image_channel_order (priv->context, CL_MEM_OBJECT_IMAGE2D);
    g_debug ("INFO Image channel order for CL_MEM_OBJECT_IMAGE2D: %s",
             priv->channel_order_2d == CL_R ? "CL_R" : priv->channel_order_2d == CL_LUMINANCE ? "CL_LUMINANCE" : "CL_INTENSITY");
    priv->channel_order_3d = get_image_channel_order (priv->context, CL_MEM_OBJECT_IMAGE3D);
    g_debug ("INFO Image channel order for CL_MEM_OBJECT_IMAGE3D: %s",
             priv->channel_order_2d == CL_R ? "CL_R" : priv->channel_order_2d == CL_LUMINANCE ? "CL_LUMINANCE" : "CL_INTENSITY");
    priv->gpu_nodes = NULL;
    priv->device_names = g_malloc0 (priv->n_devices * sizeof (gchar *));

    for (guint i = 0; i < priv->n_devices; i++) {
        UfoGpuNode *node;
        size_t size;

        node = UFO_GPU_NODE (ufo_gpu_node_new (priv->context, priv->devices[i]));

        UFO_RESOURCES_CHECK_CLERR (clGetDeviceInfo (priv->devices[i], CL_DEVICE_NAME, 0, NULL, &size));
        priv->device_names[i] = g_malloc0 (size);

        UFO_RESOURCES_CHECK_CLERR (clGetDeviceInfo (priv->devices[i], CL_DEVICE_NAME, size, priv->device_names[i], NULL));

        g_debug("NEW  UfoGpuNode-%p [device=%s]", (gpointer) node, priv->device_names[i]);
        priv->gpu_nodes = g_list_append (priv->gpu_nodes, node);
    }

    return TRUE;
}

/**
 * ufo_resources_new:
 * @error: Location of a #GError or %NULL
 *
 * Create a new #UfoResources instance.
 *
 * Returns: (transfer none): A new #UfoResources
 */
UfoResources *
ufo_resources_new (GError **error)
{
    return g_initable_new (UFO_TYPE_RESOURCES, NULL, error, NULL);
}

/**
 * ufo_resources_add_path:
 * @resources: A #UfoResources object
 * @path: a path to OpenCL kernel files
 */
void
ufo_resources_add_path (UfoResources *resources,
                        const gchar *path)
{
    g_return_if_fail (UFO_IS_RESOURCES (resources));
    g_return_if_fail (path != NULL);

    resources->priv->paths = g_list_append (resources->priv->paths, g_strdup (path));
}

static void
append_include_path (const gchar *path,
                     GString *str)
{
    g_string_append_printf (str, " -I%s", path);
}

static void
opt_append_include_paths (GString *str,
                          UfoResourcesPrivate *priv)

{
    g_list_foreach (priv->paths, (GFunc) append_include_path, str);
}

static void
opt_append_device_options (GString *str,
                           UfoResourcesPrivate *priv,
                           guint device_index)
{
    g_assert (device_index < priv->n_devices);
    g_string_append_printf (str, " -DDEVICE_%s", ufo_escape_device_name (priv->device_names[device_index]));
}

static void
handle_build_error (cl_program program,
                    cl_device_id device,
                    cl_int errcode,
                    GError **error)
{
    gsize log_size;
    gchar *log;

    g_set_error (error, UFO_RESOURCES_ERROR, UFO_RESOURCES_ERROR_BUILD_PROGRAM,
                 "Failed to build OpenCL program: %s", ufo_resources_clerr (errcode));

    UFO_RESOURCES_CHECK_CLERR (clGetProgramBuildInfo (program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size));

    log = g_malloc0 (log_size * sizeof (char));

    UFO_RESOURCES_CHECK_CLERR (clGetProgramBuildInfo (program, device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL));
    g_printerr ("\n=== Build log ===\n%s\n", log);
    g_free (log);
}

static cl_program
add_program_from_source (UfoResourcesPrivate *priv,
                         const gchar *source,
                         const gchar *options,
                         GError **error)
{
    cl_program program;
    cl_int errcode = CL_SUCCESS;
    GTimer *timer;

    program = g_hash_table_lookup (priv->programs, source);

    if (program != NULL)
        return program;

    program = clCreateProgramWithSource (priv->context, 1, &source, NULL, &errcode);

    if (errcode != CL_SUCCESS) {
        g_set_error (error, UFO_RESOURCES_ERROR, UFO_RESOURCES_ERROR_CREATE_PROGRAM,
                     "Failed to create OpenCL program: %s", ufo_resources_clerr (errcode));
        return NULL;
    }

    timer = g_timer_new ();

    for (guint i = 0; i < priv->n_devices; i++) {
        GString *build_options;

        build_options = g_string_new (priv->build_opts->str);
        opt_append_device_options (build_options, priv, i);
        opt_append_include_paths (build_options, priv);

        if (options != NULL) {
            gchar *stripped_opts = g_strstrip (g_strdup (options));
            g_string_append (build_options, " ");
            g_string_append (build_options, stripped_opts);
            g_free (stripped_opts);
        }

        errcode = clBuildProgram (program, 1, &priv->devices[i], build_options->str, NULL, NULL);

        if (errcode != CL_SUCCESS) {
            handle_build_error (program, priv->devices[0], errcode, error);
            return NULL;
        }

        if (options)
            g_debug ("INFO Built with `%s %s' for device %i", build_options->str, options, i);
        else
            g_debug ("INFO Built with `%s' for device %i", build_options->str, i);

        g_string_free (build_options, TRUE);
    }

    g_timer_stop (timer);
    g_debug ("INFO Built program for %i devices in %3.5fs", priv->n_devices, g_timer_elapsed (timer, NULL));
    g_timer_destroy (timer);
    g_hash_table_insert (priv->programs, g_strdup (source), program);

    return program;
}

static gchar *
get_first_kernel_name (const gchar *source)
{
    GRegex *regex;
    gchar *name = NULL;
    GMatchInfo *match = NULL;
    GError *error = NULL;

    regex = g_regex_new ("__kernel\\svoid\\s([A-Za-z][A-Za-z0-9_]+)",
                         G_REGEX_MULTILINE, 0, &error);

    if (error != NULL) {
        g_error ("%s", error->message);
        g_error_free (error);
        return NULL;
    }

    if (g_regex_match (regex, source, 0, &match))
        name = g_match_info_fetch (match, 1);

    g_match_info_free (match);
    g_regex_unref (regex);
    return name;
}

static cl_kernel
create_kernel (UfoResourcesPrivate *priv,
               cl_program program,
               const gchar *kernel_name,
               GError **error)
{
    cl_kernel kernel;
    gchar *name;
    cl_int errcode = CL_SUCCESS;

    if (kernel_name == NULL) {
        gchar *source;
        gsize size;

        UFO_RESOURCES_CHECK_CLERR (clGetProgramInfo (program, CL_PROGRAM_SOURCE, 0, NULL, &size));
        source = g_malloc0 (size);
        UFO_RESOURCES_CHECK_CLERR (clGetProgramInfo (program, CL_PROGRAM_SOURCE, size, source, NULL));
        name = get_first_kernel_name (source);
        g_free (source);
    }
    else {
        name = g_strdup (kernel_name);
    }

    kernel = clCreateKernel (program, name, &errcode);
    g_free (name);

    if (kernel == NULL || errcode != CL_SUCCESS) {
        g_set_error (error, UFO_RESOURCES_ERROR, UFO_RESOURCES_ERROR_CREATE_KERNEL,
                     "Failed to create kernel `%s`: %s", kernel_name, ufo_resources_clerr (errcode));
        return NULL;
    }

    priv->kernels = g_list_append (priv->kernels, kernel);
    return kernel;
}

static gchar *
create_cache_key (const gchar *filename,
                  const gchar *kernelname)
{
    return g_strdup_printf ("%s:%s", filename, kernelname);
}

/**
 * ufo_resources_get_kernel:
 * @resources: A #UfoResources object
 * @filename: Name of the .cl kernel file
 * @kernel: Name of a kernel, or %NULL
 * @options: Options passed to the OpenCL compiler or %NULL
 * @error: Return location for a GError from #UfoResourcesError, or %NULL
 *
 * Loads and builds a kernel from a file. The file is searched in the current
 * working directory and all paths added through ufo_resources_add_path (). If
 * @kernel is %NULL, the first encountered kernel is returned. Options are
 * passed verbatim to the OpenCL compiler and ignored if %NULL.
 *
 * Returns: (transfer none): a cl_kernel object that is load from @filename or
 *  %NULL on error
 */
gpointer
ufo_resources_get_kernel (UfoResources *resources,
                          const gchar *filename,
                          const gchar *kernel,
                          const gchar *options,
                          GError **error)
{
    UfoResourcesPrivate *priv;
    gchar *path;
    gchar *buffer;
    cl_program program;
    cl_kernel result;

    g_return_val_if_fail (UFO_IS_RESOURCES (resources) && (filename != NULL), NULL);

    result = NULL;
    buffer = NULL;
    priv = resources->priv;
    path = lookup_kernel_path (priv, filename);

    if (path == NULL) {
        g_set_error (error, UFO_RESOURCES_ERROR, UFO_RESOURCES_ERROR_LOAD_PROGRAM,
                     "Could not find `%s'. Use add_paths() to add additional kernel paths", filename);
        return NULL;
    }

    buffer = read_file (path);

    if (buffer == NULL) {
        g_set_error (error, UFO_RESOURCES_ERROR, UFO_RESOURCES_ERROR_LOAD_PROGRAM,
                     "Could not open `%s'", filename);
        goto exit;
    }

    program = add_program_from_source (priv, buffer, options, error);

    if (program == NULL)
        goto exit;

    g_debug ("INFO Compiled `%s' kernel from %s", kernel, path);
    result = create_kernel (priv, program, kernel, error);

exit:
    g_free (buffer);
    g_free (path);

    return result;
}

/**
 * ufo_resources_get_kernel_from_source:
 * @resources: A #UfoResources
 * @source: OpenCL source string
 * @kernel: Name of a kernel or %NULL
 * @options: Options passed to the OpenCL compiler
 * @error: Return location for a GError from #UfoResourcesError, or NULL
 *
 * Loads and builds a kernel from a string. If @kernel is %NULL, the first
 * kernel defined in @source is used.
 *
 * Returns: (transfer none): a cl_kernel object that is load from @filename
 */
gpointer
ufo_resources_get_kernel_from_source (UfoResources *resources,
                                      const gchar *source,
                                      const gchar *kernel,
                                      const gchar *options,
                                      GError **error)
{
    UfoResourcesPrivate *priv;
    cl_program program;

    g_return_val_if_fail (UFO_IS_RESOURCES (resources) && (source != NULL), NULL);

    priv = UFO_RESOURCES_GET_PRIVATE (resources);
    program = add_program_from_source (priv, source, options, error);

    if (program == NULL)
        return NULL;

    g_debug ("INFO Added program %p from source", (gpointer) program);
    return create_kernel (priv, program, kernel, error);
}

/**
 * ufo_resources_get_cached_kernel:
 * @resources: A #UfoResources object
 * @filename: Name of the .cl kernel file
 * @kernel: Name of a kernel, or %NULL
 * @error: Return location for a GError from #UfoResourcesError, or %NULL
 *
 * Loads a and builds a kernel from a file. The file is searched in the current
 * working directory and all paths added through ufo_resources_add_path (). If
 * @kernel is %NULL, the first encountered kernel is returned. The kernel object
 * is cached and should not be used by two threads concurrently.
 *
 * Returns: (transfer none): a cl_kernel object that is load from @filename or
 *  %NULL on error
 */
gpointer
ufo_resources_get_cached_kernel (UfoResources *resources,
                                 const gchar *filename,
                                 const gchar *kernelname,
                                 GError **error)
{
    UfoResourcesPrivate *priv;
    cl_kernel kernel;

    g_return_val_if_fail (UFO_IS_RESOURCES (resources) &&
                          (filename != NULL), NULL);

    priv = resources->priv;

    if (kernelname != NULL) {
        gchar *cache_key;

        cache_key = create_cache_key (filename, kernelname);
        kernel = g_hash_table_lookup (priv->kernel_cache, cache_key);

        if (kernel != NULL) {
            g_free (cache_key);
            return kernel;
        }
    }

    kernel = ufo_resources_get_kernel (resources, filename, kernelname, NULL, error);

    if (kernel != NULL && kernelname != NULL) {
        gchar *cache_key;

        cache_key = create_cache_key (filename, kernelname);
        g_hash_table_insert (priv->kernel_cache, cache_key, kernel);
    }

    return kernel;
}

/**
 * ufo_resources_get_kernel_source:
 * @resources: A #UfoResources object
 * @filename: Name of the .cl kernel file
 * @error: Return location for a GError from #UfoResourcesError, or %NULL
 *
 * Loads a file present in the kernel PATH search list. The file is searched in the current
 * working directory and all paths added through ufo_resources_add_path ().
 *
 * Returns: (transfer none): a string (gchar*) load from @filename or %NULL on error, user is
 * responsible for free() it after using it.
 */
gchar *
ufo_resources_get_kernel_source (UfoResources   *resources,
                                 const gchar    *filename,
                                 GError        **error)
{
    UfoResourcesPrivate *priv;
    gchar *path;
    gchar *buffer;

    g_return_val_if_fail (UFO_IS_RESOURCES (resources) && (filename != NULL), NULL);

    buffer = NULL;
    priv = resources->priv;
    path = lookup_kernel_path (priv, filename);

    if (path == NULL) {
        g_set_error (error, UFO_RESOURCES_ERROR, UFO_RESOURCES_ERROR_LOAD_PROGRAM,
                     "Could not find `%s'. Use add_paths() to add additional kernel paths", filename);
        return NULL;
    }

    buffer = read_file (path);

    if (buffer == NULL) {
        g_set_error (error, UFO_RESOURCES_ERROR, UFO_RESOURCES_ERROR_LOAD_PROGRAM,
                     "Could not open `%s'", filename);
    }

    g_free (path);
    return buffer;
}


/**
 * ufo_resources_get_context: (skip)
 * @resources: A #UfoResources
 *
 * Returns the OpenCL context object that is used by the resource resources. This
 * context can be used to initialize othe third-party libraries.
 *
 * Return value: A cl_context object.
 */
gpointer
ufo_resources_get_context (UfoResources *resources)
{
    g_return_val_if_fail (UFO_IS_RESOURCES (resources), NULL);
    return resources->priv->context;
}


/**
 * ufo_resources_get_channel_order_2d: (skip)
 * @resources: A #UfoResources
 *
 * Returns the cl_channel_order which can be used in combination with CL_FLOAT
 * cl_channel_data_type and CL_MEM_OBJECT_IMAGE2D image type.
 *
 * Return value: cl_channel_order.
 */
unsigned int
ufo_resources_get_channel_order_2d (UfoResources *resources)
{
    g_return_val_if_fail (UFO_IS_RESOURCES (resources), 0);
    return resources->priv->channel_order_2d;
}

/**
 * ufo_resources_get_channel_order_3d: (skip)
 * @resources: A #UfoResources
 *
 * Returns the cl_channel_order which can be used in combination with CL_FLOAT
 * cl_channel_data_type and CL_MEM_OBJECT_IMAGE3D image type.
 *
 * Return value: cl_channel_order.
 */
unsigned int
ufo_resources_get_channel_order_3d (UfoResources *resources)
{
    g_return_val_if_fail (UFO_IS_RESOURCES (resources), 0);
    return resources->priv->channel_order_3d;
}

/**
 * ufo_resources_get_cmd_queues: (skip)
 * @resources: A #UfoResources
 *
 * Get all command queues managed by @resources.
 *
 * Returns: (transfer container) (element-type gpointer): List with
 * cl_command_queue objects. Free with g_list_free() but not its elements.
 */
GList *
ufo_resources_get_cmd_queues (UfoResources *resources)
{
    UfoResourcesPrivate *priv;
    GList *it;
    GList *result = NULL;

    g_return_val_if_fail (UFO_IS_RESOURCES (resources), NULL);
    priv = UFO_RESOURCES_GET_PRIVATE (resources);

    g_list_for (priv->gpu_nodes, it) {
        result = g_list_append (result, ufo_gpu_node_get_cmd_queue (UFO_GPU_NODE (it->data)));
    }

    return result;
}

/**
 * ufo_resources_get_devices: (skip)
 * @resources: A #UfoResources
 *
 * Get all devices queues managed by @resources.
 *
 * Returns: (element-type gpointer) (transfer container): List with
 * cl_device_id objects. Free with g_list_free() but not its elements.
 */
GList *
ufo_resources_get_devices (UfoResources *resources)
{
    UfoResourcesPrivate *priv;
    GList *result = NULL;

    g_return_val_if_fail (UFO_IS_RESOURCES(resources), NULL);
    priv = resources->priv;

    for (guint i = 0; i < priv->n_devices; i++)
        result = g_list_append (result, priv->devices[i]);

    return result;
}

/**
 * ufo_resources_get_gpu_nodes:
 * @resources: A #UfoResources
 *
 * Get all #UfoGpuNode objects managed by @resources.
 *
 * Returns: (transfer container) (element-type Ufo.GpuNode): List with
 * #UfoGpuNode objects. Free with g_list_free() but not its elements.
 */
GList *
ufo_resources_get_gpu_nodes (UfoResources *resources)
{
    g_return_val_if_fail (UFO_IS_RESOURCES (resources), NULL);
    return g_list_copy (resources->priv->gpu_nodes);
}

static void
ufo_resources_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    UfoResourcesPrivate *priv = UFO_RESOURCES_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_PLATFORM_INDEX:
            priv->platform_index = g_value_get_int (value);
            break;

        case PROP_DEVICE_TYPE:
            priv->device_type = g_value_get_flags (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_resources_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    UfoResourcesPrivate *priv = UFO_RESOURCES_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_PLATFORM_INDEX:
            g_value_set_int (value, priv->platform_index);
            break;

        case PROP_DEVICE_TYPE:
            g_value_set_flags (value, priv->device_type);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_resources_dispose (GObject *object)
{
    UfoResourcesPrivate *priv;
    GList *it;

    priv = UFO_RESOURCES_GET_PRIVATE (object);

    g_list_for (priv->gpu_nodes, it) {
        g_object_unref (G_OBJECT (it->data));
    }

    g_list_free (priv->gpu_nodes);

    priv->gpu_nodes = NULL;
}

static void
ufo_resources_finalize (GObject *object)
{
    UfoResourcesPrivate *priv;

    priv = UFO_RESOURCES_GET_PRIVATE (object);

    g_clear_error (&priv->construct_error);
    g_hash_table_destroy (priv->kernel_cache);

    g_list_free_full (priv->paths, g_free);
    g_list_free_full (priv->kernels, (GDestroyNotify) release_kernel);

    g_hash_table_destroy (priv->programs);

    if (priv->device_names != NULL) {
        for (guint i = 0; i < priv->n_devices; i++)
            g_free (priv->device_names[i]);
    }

    if (priv->context) {
        g_debug ("FREE context=%p", (gpointer) priv->context);
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
    }

    g_string_free (priv->build_opts, TRUE);

    g_free (priv->device_names);
    g_free (priv->devices);

    priv->kernels = NULL;
    priv->devices = NULL;

    G_OBJECT_CLASS (ufo_resources_parent_class)->finalize (object);
}

static gboolean
ufo_resources_initable_init (GInitable *initable,
                             GCancellable *cancellable,
                             GError **error)
{
    UfoResources *resources;
    UfoResourcesPrivate *priv;

    g_return_val_if_fail (UFO_IS_RESOURCES (initable), FALSE);

    if (cancellable != NULL) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                             "Cancellable initialization not supported");
        return FALSE;
    }

    resources = UFO_RESOURCES (initable);
    priv = resources->priv;

    if (priv->construct_error != NULL) {
        /* 
         * For some very strange reason, using g_propagate_error() does not
         * work, the error variable is set but does not contain the message.
         */
        g_set_error_literal (error, UFO_RESOURCES_ERROR, UFO_RESOURCES_ERROR_GENERAL,
                             priv->construct_error->message);
        return FALSE;
    }

    return TRUE;
}

static void
ufo_resources_initable_iface_init (GInitableIface *iface)
{
    iface->init = ufo_resources_initable_init;
}

static void
ufo_resources_class_init (UfoResourcesClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_resources_set_property;
    oclass->get_property = ufo_resources_get_property;
    oclass->dispose = ufo_resources_dispose;
    oclass->finalize = ufo_resources_finalize;

    /**
     * UfoResources:platform:
     *
     * Let the user select which device class to use for execution.
     *
     * See: #UfoDeviceType for the device classes.
     */
    properties[PROP_PLATFORM_INDEX] =
        g_param_spec_int ("platform-index",
                          "Platform index to use",
                          "Platform index, -1 denotes any platform",
                           -1, 16, 0,
                           G_PARAM_READWRITE);

    /**
     * UfoResources:device-type:
     *
     * Type of the devices that should be used exclusively for computation.
     *
     * See: #UfoDeviceType for the device classes.
     */
    properties[PROP_DEVICE_TYPE] =
        g_param_spec_flags ("device-type",
                            "Device type for all employed devices",
                            "Device type for all employed devices",
                            UFO_TYPE_DEVICE_TYPE, UFO_DEVICE_GPU,
                            G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (klass, sizeof (UfoResourcesPrivate));
}

static void
ufo_resources_init (UfoResources *self)
{
    UfoResourcesPrivate *priv;
    const gchar *kernel_path;
    gchar **kernel_paths;
    gchar **path;

    self->priv = priv = UFO_RESOURCES_GET_PRIVATE (self);

    priv->construct_error = NULL;
    priv->programs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) release_program);
    priv->kernels = NULL;
    priv->kernel_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    priv->build_opts = g_string_new ("-cl-mad-enable ");

    priv->paths = g_list_append (NULL, g_strdup ("."));
    priv->paths = g_list_append (priv->paths, g_strdup (UFO_KERNEL_DIR));
    priv->gpu_nodes = NULL;

    kernel_path = g_getenv ("UFO_KERNEL_PATH");

    if (kernel_path != NULL) {
        kernel_paths = g_strsplit (kernel_path, ":", -1);

        for (path = kernel_paths; *path; path++) {
            if (**path) {
                priv->paths = g_list_append (priv->paths, *path);
            }
        }

        g_free (kernel_paths);
    }

    priv->device_type = UFO_DEVICE_GPU;
    priv->platform_index = -1;

    initialize_opencl (priv);
}
