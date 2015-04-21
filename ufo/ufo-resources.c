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
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <ufo/ufo-resources.h>
#include <ufo/ufo-gpu-node.h>
#include <ufo/ufo-remote-node.h>
#include <ufo/ufo-enums.h>
#include "compat.h"

/**
 * SECTION:ufo-resources
 * @Short_description: Manage OpenCL resources
 * @Title: UfoResources
 *
 * The #UfoResources creates the OpenCL environment and loads OpenCL kernels
 * from text files. Users should in general not create a resources object
 * themselves but use one that is created automatically by #UfoArchGraph.
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
    cl_uint          n_devices;         /* Number of OpenCL devices per platform id */
    cl_device_id     *devices;          /* Array of OpenCL devices per platform id */

    GList       *gpu_nodes;

    GList       *paths;         /* List of paths containing kernels and header files */
    GHashTable  *kernel_cache;
    GList       *programs;
    GList       *kernels;
    GString     *build_opts;

    GList       *remotes;
    GList       *remote_nodes;
};

enum {
    PROP_0,
    PROP_PLATFORM_INDEX,
    PROP_DEVICE_TYPE,
    PROP_REMOTES,
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
    FILE *fp = fopen (filename, "r");

    if (fp == NULL)
        return NULL;

    fseek (fp, 0, SEEK_END);
    const gsize length = (gsize) ftell (fp);
    rewind (fp);
    gchar *buffer = (gchar *) g_malloc0(length + 1);

    if (buffer == NULL) {
        fclose (fp);
        return NULL;
    }

    size_t buffer_length = fread (buffer, 1, length, fp);
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
    g_debug ("Release kernel=%p", (gpointer) kernel);
    UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (kernel));
}

static void
release_program (cl_program program)
{
    g_debug ("Release program=%p", (gpointer) program);
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

    UFO_RESOURCES_CHECK_CLERR (clGetPlatformIDs (0, NULL, &n_platforms));
    platforms = g_malloc0 (n_platforms * sizeof (cl_platform_id));
    UFO_RESOURCES_CHECK_CLERR (clGetPlatformIDs (n_platforms, platforms, NULL));

    g_debug ("Found %i OpenCL platforms %i", n_platforms, priv->platform_index);

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
add_vendor_to_build_opts (GString *opts,
                          cl_platform_id platform)
{
    if (platform_vendor_has_prefix (platform, "NVIDIA"))
        g_string_append (opts, "-cl-nv-verbose -DVENDOR=NVIDIA");

    if (platform_vendor_has_prefix (platform, "Advanced Micro Devices"))
        g_string_append (opts, "-DVENDOR=AMD");
}

static void
restrict_to_gpu_subset (UfoResourcesPrivate *priv)
{
    /*
     * Selects a single GPU which can be set via the UFO_USE_GPU
     * environment variable, even if more GPUs are available. The specifc GPU
     * is selected via the integer value of UFO_USE_GPU (index starts at 1).
     * Used for debugging and evaluation.
     */

    const gchar* env_gpu = g_getenv ("UFO_USE_GPU");

    if (env_gpu == NULL || g_strcmp0 (env_gpu, "") == 0)
        return;

    guint device_index = (guint) g_ascii_strtoull (env_gpu, NULL, 0);

    if (device_index == 0) {
        g_error ("Unrecognized format for env var UFO_USE_GPU");
        return;
    }

    if (device_index > priv->n_devices) {
        g_error ("Can't select UFO_USE_GPU=%d gpus as it exceeds number of available devices", device_index);
        return;
    }

    // TODO allow restriction to real subset, like 1,3,5 etc.
    cl_device_id *devices_subset = g_malloc0 (1 * sizeof (cl_device_id));
    devices_subset[0] = priv->devices[device_index - 1];
    g_free (priv->devices);
    priv->devices = devices_subset;
    priv->n_devices = 1;
}

static gboolean
initialize_opencl (UfoResourcesPrivate *priv)
{
    cl_device_type device_type;
    cl_int errcode = CL_SUCCESS;

    priv->platform = get_preferably_gpu_based_platform (priv);
    add_vendor_to_build_opts (priv->build_opts, priv->platform);

    device_type = 0;
    device_type |= priv->device_type && UFO_DEVICE_CPU ? CL_DEVICE_TYPE_CPU : 0;
    device_type |= priv->device_type && UFO_DEVICE_GPU ? CL_DEVICE_TYPE_GPU : 0;
    device_type |= priv->device_type && UFO_DEVICE_ACC ? CL_DEVICE_TYPE_ACCELERATOR : 0;

    errcode = clGetDeviceIDs (priv->platform, device_type, 0, NULL, &priv->n_devices);
    UFO_RESOURCES_CHECK_AND_SET (errcode, &priv->construct_error);

    g_debug ("Platform `%p' has %i devices", (gpointer) priv->platform, priv->n_devices);

    if (errcode != CL_SUCCESS)
        return FALSE;

    priv->devices = g_malloc0 (priv->n_devices * sizeof (cl_device_id));

    errcode = clGetDeviceIDs (priv->platform, device_type, priv->n_devices, priv->devices, NULL);
    UFO_RESOURCES_CHECK_AND_SET (errcode, &priv->construct_error);

    if (errcode != CL_SUCCESS)
        return FALSE;

    restrict_to_gpu_subset (priv);

    priv->context = clCreateContext (NULL,
                                     priv->n_devices, priv->devices,
                                     NULL, NULL, &errcode);

    UFO_RESOURCES_CHECK_AND_SET (errcode, &priv->construct_error);

    if (errcode != CL_SUCCESS)
        return FALSE;

    priv->gpu_nodes = NULL;

    for (guint i = 0; i < priv->n_devices; i++) {
        priv->gpu_nodes = g_list_append (priv->gpu_nodes, ufo_gpu_node_new (priv->context, priv->devices[i]));
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

void
ufo_resources_add_path (UfoResources *resources,
                        const gchar *path)
{
    g_return_if_fail (UFO_IS_RESOURCES (resources));
    g_return_if_fail (path != NULL);

    resources->priv->paths = g_list_append (resources->priv->paths, g_strdup (path));
}

static gchar *
escape_device_name (gchar *name)
{
    gchar *tmp = name;

    while (*tmp) {
        gchar c = *tmp;

        if (c == ' ')
            *tmp = '_';
        else
            *tmp = g_ascii_toupper (c);

        tmp++;
    }

    return name;
}

static void
append_include_path (const gchar *path,
                     GString *str)
{
    g_string_append_printf (str, " -I%s", path);
}

static gchar *
get_device_build_options (UfoResourcesPrivate *priv,
                          guint device_index,
                          const gchar *additional)
{
    GString *opts;
    gsize size;
    gchar *name;

    g_assert (device_index < priv->n_devices);

    opts = g_string_new (priv->build_opts->str);

    if (additional != NULL)
        g_string_append (opts, additional);

    UFO_RESOURCES_CHECK_CLERR (clGetDeviceInfo (priv->devices[device_index],
                                                CL_DEVICE_NAME, 0, NULL, &size));
    name = g_malloc0 (size);

    UFO_RESOURCES_CHECK_CLERR (clGetDeviceInfo (priv->devices[device_index],
                                                CL_DEVICE_NAME, size, name, NULL));

    g_string_append_printf (opts, " -DDEVICE=%s", escape_device_name (name));
    g_free (name);

    g_list_foreach (priv->paths, (GFunc) append_include_path, opts);

    return g_string_free (opts, FALSE);
}

static void
handle_build_error (cl_program program,
                    cl_device_id device,
                    cl_int errcode,
                    GError **error)
{
    gsize log_size;
    gchar *log;

    g_set_error (error,
                 UFO_RESOURCES_ERROR,
                 UFO_RESOURCES_ERROR_BUILD_PROGRAM,
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
    gchar *build_options;

    program = clCreateProgramWithSource (priv->context,
                                         1, &source, NULL, &errcode);

    if (errcode != CL_SUCCESS) {
        g_set_error (error,
                     UFO_RESOURCES_ERROR,
                     UFO_RESOURCES_ERROR_CREATE_PROGRAM,
                     "Failed to create OpenCL program: %s", ufo_resources_clerr (errcode));
        return NULL;
    }

    build_options = get_device_build_options (priv, 0, options);

    errcode = clBuildProgram (program,
                              priv->n_devices, priv->devices,
                              build_options,
                              NULL, NULL);

    if (errcode != CL_SUCCESS) {
        handle_build_error (program, priv->devices[0], errcode, error);
        return NULL;
    }

    priv->programs = g_list_append (priv->programs, program);

    g_free (build_options);
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
        g_set_error (error,
                     UFO_RESOURCES_ERROR,
                     UFO_RESOURCES_ERROR_CREATE_KERNEL,
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
 * ufo_resources_get_kernel_with_opts:
 * @resources: A #UfoResources object
 * @filename: Name of the .cl kernel file
 * @kernel: Name of a kernel, or %NULL
 * @options: Options passed to the OpenCL compiler
 * @error: Return location for a GError from #UfoResourcesError, or %NULL
 *
 * Loads a and builds a kernel from a file. The file is searched in the current
 * working directory and all paths added through ufo_resources_add_paths (). If
 * @kernel is %NULL, the first encountered kernel is returned.
 *
 * Returns: (transfer none): a cl_kernel object that is load from @filename or %NULL on error
 */
gpointer
ufo_resources_get_kernel_with_opts (UfoResources   *resources,
                                    const gchar    *filename,
                                    const gchar    *kernel,
                                    const gchar    *options,
                                    GError        **error)
{
    UfoResourcesPrivate *priv;
    gchar *path;
    gchar *buffer;
    cl_program program;

    g_return_val_if_fail (UFO_IS_RESOURCES (resources) &&
                          (filename != NULL), NULL);

    priv = resources->priv;
    path = lookup_kernel_path (priv, filename);

    if (path == NULL) {
        g_set_error (error, UFO_RESOURCES_ERROR, UFO_RESOURCES_ERROR_LOAD_PROGRAM,
                     "Could not find `%s'. Use add_paths() to add additional kernel paths", filename);
        return NULL;
    }

    buffer = read_file (path);
    g_free (path);

    if (buffer == NULL) {
        g_set_error (error, UFO_RESOURCES_ERROR, UFO_RESOURCES_ERROR_LOAD_PROGRAM,
                     "Could not open `%s'", filename);
        return NULL;
    }

    program = add_program_from_source (priv, buffer, options, error);

    if (program == NULL)
        return NULL;

    g_debug ("Added program %p from `%s`", (gpointer) program, filename);
    g_free (buffer);

    return create_kernel (priv, program, kernel, error);
}

/**
 * ufo_resources_get_kernel:
 * @resources: A #UfoResources object
 * @filename: Name of the .cl kernel file
 * @kernel: Name of a kernel, or %NULL
 * @error: Return location for a GError from #UfoResourcesError, or %NULL
 *
 * Loads a and builds a kernel from a file. The file is searched in the current
 * working directory and all paths added through ufo_resources_add_path (). If
 * @kernel is %NULL, the first encountered kernel is returned.
 *
 * Returns: (transfer none): a cl_kernel object that is load from @filename or %NULL on error
 */
gpointer
ufo_resources_get_kernel (UfoResources *resources,
                          const gchar *filename,
                          const gchar *kernel,
                          GError **error)
{
    return ufo_resources_get_kernel_with_opts (resources, filename, kernel, "", error);
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
 * Returns: (transfer none): a cl_kernel object that is load from @filename or %NULL on error
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

    kernel = ufo_resources_get_kernel (resources, filename, kernelname, error);

    if (kernel != NULL && kernelname != NULL) {
        gchar *cache_key;

        cache_key = create_cache_key (filename, kernelname);
        g_hash_table_insert (priv->kernel_cache, cache_key, kernel);
    }

    return kernel;
}

/**
 * ufo_resources_get_kernel_from_source:
 * @resources: A #UfoResources
 * @source: OpenCL source string
 * @kernel: Name of a kernel or %NULL
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
                                      GError **error)
{
    UfoResourcesPrivate *priv;
    cl_program program;

    g_return_val_if_fail (UFO_IS_RESOURCES (resources) &&
                          (source != NULL), NULL);

    priv = UFO_RESOURCES_GET_PRIVATE (resources);
    program = add_program_from_source (priv, source, NULL, error);

    if (program == NULL)
        return NULL;

    g_debug ("Added program %p from source", (gpointer) program);
    return create_kernel (priv, program, kernel, error);
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

/**
 * ufo_resources_get_remote_nodes:
 * @resources: A #UfoResources
 *
 * Get all #UfoRemoteNode objects managed by @resources.
 *
 * Returns: (transfer container) (element-type Ufo.RemoteNode): List with
 * #UfoRemoteNode objects. Free with g_list_free() but not its elements.
 */
GList *
ufo_resources_get_remote_nodes (UfoResources *resources)
{
    g_return_val_if_fail (UFO_IS_RESOURCES (resources), NULL);
    return g_list_copy (resources->priv->remote_nodes);
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

        case PROP_REMOTES:
            {
                GValueArray *array;

                g_assert (priv->remotes == NULL);
                array = g_value_get_boxed (value);

                if (array != NULL) {
                    g_list_free (priv->remotes);

                    for (guint i = 0; i < array->n_values; i++) {
                        gchar *address;
                        UfoNode *node;

                        address = g_strdup (g_value_get_string (&array->values[i]));
                        node = ufo_remote_node_new (address);
                        priv->remotes = g_list_append (priv->remotes, address);
                        priv->remote_nodes = g_list_append (priv->remote_nodes, node);
                    }
                }
            }
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

        case PROP_REMOTES:
            g_value_set_boxed (value, priv->remotes);
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

    g_list_for (priv->remote_nodes, it) {
        g_object_unref (G_OBJECT (it->data));
    }

    g_list_free (priv->gpu_nodes);
    g_list_free (priv->remote_nodes);

    priv->gpu_nodes = NULL;
    priv->remote_nodes = NULL;
}

static void
ufo_resources_finalize (GObject *object)
{
    UfoResourcesPrivate *priv;

    priv = UFO_RESOURCES_GET_PRIVATE (object);

    g_clear_error (&priv->construct_error);
    g_hash_table_destroy (priv->kernel_cache);

    g_list_free_full (priv->remotes, g_free);
    g_list_free_full (priv->paths, g_free);
    g_list_free_full (priv->kernels, (GDestroyNotify) release_kernel);
    g_list_free_full (priv->programs, (GDestroyNotify) release_program);

    if (priv->context)
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));

    g_string_free (priv->build_opts, TRUE);

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

    if (priv->construct_error != NULL)
        return FALSE;

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
                            UFO_TYPE_DEVICE_TYPE, UFO_DEVICE_ALL,
                            G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    properties[PROP_REMOTES] =
        g_param_spec_value_array ("remotes",
                                  "List with remote addresses",
                                  "List with remote addresses",
                                  g_param_spec_string ("remote",
                                                       "Remote address",
                                                       "Remote address",
                                                       "tcp://127.0.0.1:5554",
                                                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READABLE),
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
    priv->programs = NULL;
    priv->kernels = NULL;
    priv->kernel_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    priv->build_opts = g_string_new ("-cl-mad-enable ");

    priv->paths = g_list_append (NULL, g_strdup ("."));
    priv->paths = g_list_append (priv->paths, g_strdup (UFO_KERNEL_DIR));
    priv->gpu_nodes = NULL;
    priv->remotes = NULL;
    priv->remote_nodes = NULL;

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
