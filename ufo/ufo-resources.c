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
#include <stdio.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <ufo/ufo-resources.h>
#include <ufo/ufo-configurable.h>

/**
 * SECTION:ufo-resources
 * @Short_description: Manage OpenCL resources
 * @Title: UfoResources
 *
 * The #UfoResources creates the OpenCL environment and loads OpenCL
 * kernels from text files.
 */

G_DEFINE_TYPE_WITH_CODE (UfoResources, ufo_resources, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_CONFIGURABLE, NULL))

#define UFO_RESOURCES_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_RESOURCES, UfoResourcesPrivate))

/**
 * UfoResourcesError:
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
    UfoConfig       *config;

    cl_platform_id   platform;
    cl_context       context;
    cl_uint          n_devices;         /**< Number of OpenCL devices per platform id */
    cl_device_id     *devices;          /**< Array of OpenCL devices per platform id */
    cl_command_queue *command_queues;   /**< Array of command queues per device */

    GList       *include_paths;         /**< List of include paths for kernel includes >*/
    GList       *kernel_paths;          /**< Colon-separated string with paths to kernel files */
    GHashTable  *programs;              /**< Map from filename to cl_program */
    GList       *kernels;
    GString     *build_opts;
};

enum {
    PROP_0,
    PROP_CONFIG,
    N_PROPERTIES
};

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
    UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (kernel));
}

static void
release_program (cl_program program)
{
    UFO_RESOURCES_CHECK_CLERR (clReleaseProgram (program));
}

static gchar *
lookup_kernel_path (UfoResourcesPrivate *priv,
                  const gchar *filename)
{
    /* Check first if filename is already a path */
    if (g_path_is_absolute (filename)) {
        if (g_file_test (filename, G_FILE_TEST_EXISTS))
            return g_strdup (filename);
        else
            return NULL;
    }

    for (GList *it = g_list_first (priv->kernel_paths); it != NULL; it = g_list_next (it)) {
        gchar *path;

        path = g_build_filename ((gchar *) it->data, filename, NULL);

        if (g_file_test (path, G_FILE_TEST_EXISTS))
            return path;

        g_free (path);
    }

    return NULL;
}

/**
 * ufo_resources_new:
 * @config: A #UfoConfiguration object or %NULL
 *
 * Create a new #UfoResources instance.
 *
 * Returns: (transfer none): A new #UfoResources
 */
UfoResources *
ufo_resources_new (UfoConfig *config)
{
    return UFO_RESOURCES (g_object_new (UFO_TYPE_RESOURCES,
                                        "config", config,
                                        NULL));
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

    g_string_append_printf (opts, " -D=%s", escape_device_name (name));
    g_free (name);

    return g_string_free (opts, FALSE);
}

static void
handle_build_error (cl_program program,
                    cl_device_id device,
                    cl_int errcode,
                    GError **error)
{
    const gsize LOG_SIZE = 4096;
    gchar *log;

    g_set_error (error,
                 UFO_RESOURCES_ERROR,
                 UFO_RESOURCES_ERROR_BUILD_PROGRAM,
                 "Failed to build OpenCL program: %s", ufo_resources_clerr (errcode));

    log = g_malloc0 (LOG_SIZE * sizeof (char));

    UFO_RESOURCES_CHECK_CLERR (clGetProgramBuildInfo (program, device, CL_PROGRAM_BUILD_LOG,
                                                      LOG_SIZE, log, NULL));
    g_print ("\n=== Build log for s===%s\n\n", log);
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

    program = clCreateProgramWithSource (priv->context,
                                         1, &source, NULL, &errcode);

    if (errcode != CL_SUCCESS) {
        g_set_error (error,
                     UFO_RESOURCES_ERROR,
                     UFO_RESOURCES_ERROR_CREATE_PROGRAM,
                     "Failed to create OpenCL program: %s", ufo_resources_clerr (errcode));
        return NULL;
    }


    for (guint i = 0; i < priv->n_devices; i++) {
        gchar *build_options;

        build_options = get_device_build_options (priv, i, options);
        errcode = clBuildProgram (program,
                                  1, &priv->devices[i],
                                  build_options,
                                  NULL, NULL);

        if (errcode != CL_SUCCESS) {
            handle_build_error (program, priv->devices[0], errcode, error);
            return NULL;
        }

        g_free (build_options);
    }

    return program;
}

static cl_program
create_and_build_program (UfoResources *resources,
                          const gchar *filename,
                          const gchar *options,
                          GError **error)
{
    UfoResourcesPrivate *priv;
    cl_program program;
    gchar *path;
    gchar *buffer;
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

    g_return_val_if_fail (UFO_IS_RESOURCES (resources) || (filename != NULL), FALSE);
    priv = resources->priv;

    /* Programs might be added multiple times if this is not locked */
    g_static_mutex_lock (&mutex);

    /* Don't process the kernel file again, if already load */
    program = g_hash_table_lookup (priv->programs, filename);

    if (program != NULL) {
        g_static_mutex_unlock (&mutex);
        return program;
    }

    path = lookup_kernel_path (priv, filename);

    if (path == NULL) {
        g_set_error (error,
                     UFO_RESOURCES_ERROR,
                     UFO_RESOURCES_ERROR_LOAD_PROGRAM,
                     "Could not find `%s'. Maybe you forgot to pass a configuration?", filename);
        g_static_mutex_unlock (&mutex);
        return NULL;
    }

    buffer = read_file (path);
    g_free (path);

    if (buffer == NULL) {
        g_set_error (error,
                     UFO_RESOURCES_ERROR,
                     UFO_RESOURCES_ERROR_LOAD_PROGRAM,
                     "Could not open `%s'", filename);
        g_static_mutex_unlock (&mutex);
        return NULL;
    }

    program = add_program_from_source (priv, buffer, options, error);
    g_message ("Added program %p from `%s`", (gpointer) program, filename);

    if (program != NULL)
        g_hash_table_insert (priv->programs, g_strdup (filename), program);

    g_static_mutex_unlock (&mutex);
    g_free (buffer);
    return program;
}

static cl_kernel
create_kernel (UfoResourcesPrivate *priv,
               cl_program program,
               const gchar *kernel_name,
               GError **error)
{
    cl_int errcode = CL_SUCCESS;
    cl_kernel kernel = clCreateKernel (program, kernel_name, &errcode);

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

/**
 * ufo_resources_get_kernel:
 * @resources: A #UfoResources object
 * @filename: Name of the .cl kernel file
 * @kernel: Name of a kernel
 * @error: Return location for a GError from #UfoResourcesError, or NULL
 *
 * Loads a and builds a kernel from a file. The file is searched in the current
 * working directory and all paths added through
 * ufo_resources_add_paths ().
 *
 * Returns: (transfer none): a cl_kernel object that is load from @filename or %NULL on error
 */
gpointer
ufo_resources_get_kernel (UfoResources *resources,
                          const gchar *filename,
                          const gchar *kernel,
                          GError **error)
{
    UfoResourcesPrivate *priv;
    cl_program program;
    GError *tmp_error = NULL;

    g_return_val_if_fail (UFO_IS_RESOURCES (resources) &&
                          (filename != NULL) &&
                          (kernel != NULL), NULL);

    priv = resources->priv;
    program = create_and_build_program (resources, filename, "", &tmp_error);

    if (program == NULL) {
        g_propagate_error (error, tmp_error);
        return NULL;
    }

    return create_kernel (priv, program, kernel, error);
}

/**
 * ufo_resources_get_kernel_from_source:
 * @resources: A #UfoResources
 * @source: OpenCL source string
 * @kernel: Name of a kernel
 * @error: Return location for a GError from #UfoResourcesError, or NULL
 *
 * Loads and builds a kernel from a string.
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
                          (source != NULL) &&
                          (kernel != NULL), NULL);

    priv = UFO_RESOURCES_GET_PRIVATE (resources);
    program = add_program_from_source (priv, source, NULL, error);

    /*
     * We add the program under a fake file name. This looks very brittle to me
     * (kernel name could be the same as a source filename) but it should work
     * in most cases.
     */
    if (program != NULL)
        g_hash_table_insert (priv->programs, g_strdup (kernel), program);
    else
        return NULL;

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
 * ufo_resources_get_cmd_queues:
 * @resources: A #UfoResources
 *
 * Get all command queues managed by @resources.
 *
 * Return value: (element-type gpointer) (transfer container): List with
 * cl_command_queue objects. Free with g_list_free() but not its elements.
 */
GList *
ufo_resources_get_cmd_queues (UfoResources *resources)
{
    UfoResourcesPrivate *priv;
    GList *result = NULL;

    g_return_val_if_fail (UFO_IS_RESOURCES (resources), NULL);
    priv = resources->priv;

    for (guint i = 0; i < priv->n_devices; i++)
        result = g_list_append (result, priv->command_queues[i]);

    return result;
}

static GList *
append_config_paths (GList *list, UfoConfig *config)
{
    GList *paths;

    paths = ufo_config_get_paths (config);
    return g_list_concat (list, paths);
}

static void
ufo_resources_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    UfoResourcesPrivate *priv = UFO_RESOURCES_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_CONFIG:
            {
                GObject *value_object = g_value_get_object (value);

                if (priv->config)
                    g_object_unref (priv->config);

                if (value_object != NULL) {
                    UfoConfig *config;

                    config = UFO_CONFIG (value_object);
                    priv->kernel_paths = append_config_paths (priv->kernel_paths, config);
                    priv->include_paths = append_config_paths (priv->include_paths, config);

                    g_object_ref (config);
                    priv->config = config;
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
        case PROP_CONFIG:
            g_value_set_object (value, priv->config);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_resources_dispose (GObject *object)
{
    UfoResourcesPrivate *priv = UFO_RESOURCES_GET_PRIVATE (object);

    if (priv->config) {
        g_object_unref (priv->config);
        priv->config = NULL;
    }

    G_OBJECT_CLASS (ufo_resources_parent_class)->finalize (object);
}

static void
list_free_full (GList **list,
                GFunc free_func)
{
    g_assert (list != NULL);
    g_list_foreach (*list, free_func, NULL);
    g_list_free (*list);
    *list = NULL;
}

static void
ufo_resources_finalize (GObject *object)
{
    UfoResourcesPrivate *priv = UFO_RESOURCES_GET_PRIVATE (object);

    g_hash_table_destroy (priv->programs);

    list_free_full (&priv->kernel_paths, (GFunc) g_free);
    list_free_full (&priv->include_paths, (GFunc) g_free);
    list_free_full (&priv->kernels, (GFunc) release_kernel);

    for (guint i = 0; i < priv->n_devices; i++)
        UFO_RESOURCES_CHECK_CLERR (clReleaseCommandQueue (priv->command_queues[i]));

    UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));

    g_string_free (priv->build_opts, TRUE);

    g_free (priv->devices);
    g_free (priv->command_queues);

    priv->kernels = NULL;
    priv->devices = NULL;

    G_OBJECT_CLASS (ufo_resources_parent_class)->finalize (object);
    g_debug ("UfoResources: finalized");
}

static void
ufo_resources_class_init (UfoResourcesClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_resources_set_property;
    gobject_class->get_property = ufo_resources_get_property;
    gobject_class->dispose      = ufo_resources_dispose;
    gobject_class->finalize     = ufo_resources_finalize;

    g_object_class_override_property (gobject_class, PROP_CONFIG, "config");

    g_type_class_add_private (klass, sizeof (UfoResourcesPrivate));
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

static gboolean
platform_has_gpus (cl_platform_id platform)
{
    cl_uint n_devices;

    UFO_RESOURCES_CHECK_CLERR (clGetDeviceIDs (platform,
                                               CL_DEVICE_TYPE_GPU,
                                               0, NULL, &n_devices));
    return n_devices > 0;
}

static cl_platform_id
get_preferably_gpu_based_platform (void)
{
    cl_platform_id *platforms;
    cl_uint n_platforms;
    cl_platform_id candidate;

    UFO_RESOURCES_CHECK_CLERR (clGetPlatformIDs (0, NULL, &n_platforms));
    platforms = g_malloc0 (n_platforms * sizeof (cl_platform_id));
    UFO_RESOURCES_CHECK_CLERR (clGetPlatformIDs (n_platforms, platforms, NULL));

    if (n_platforms > 0)
        candidate = platforms[0];

    for (guint i = 0; i < n_platforms; i++) {
        if (platform_has_gpus (platforms[i])) {
            candidate = platforms[i];
            break;
        }
    }

    g_free (platforms);
    return candidate;
}

static void
initialize_opencl (UfoResourcesPrivate *priv)
{
    cl_int errcode = CL_SUCCESS;
    cl_command_queue_properties queue_properties = CL_QUEUE_PROFILING_ENABLE;

    UFO_RESOURCES_CHECK_CLERR (clGetPlatformIDs (1, &priv->platform, NULL));

    add_vendor_to_build_opts (priv->build_opts, priv->platform);

    priv->platform = get_preferably_gpu_based_platform ();

    UFO_RESOURCES_CHECK_CLERR (clGetDeviceIDs (priv->platform,
                                               CL_DEVICE_TYPE_ALL,
                                               0, NULL,
                                               &priv->n_devices));

    priv->devices = g_malloc0 (priv->n_devices * sizeof (cl_device_id));

    UFO_RESOURCES_CHECK_CLERR (clGetDeviceIDs (priv->platform,
                                               CL_DEVICE_TYPE_ALL,
                                               priv->n_devices, priv->devices,
                                               NULL));

    priv->context = clCreateContext (NULL,
                                     priv->n_devices, priv->devices,
                                     NULL, NULL, &errcode);

    UFO_RESOURCES_CHECK_CLERR (errcode);

    priv->command_queues = g_malloc0 (priv->n_devices * sizeof (cl_command_queue));

    for (guint i = 0; i < priv->n_devices; i++) {
        priv->command_queues[i] = clCreateCommandQueue (priv->context,
                                                        priv->devices[i],
                                                        queue_properties, &errcode);
        UFO_RESOURCES_CHECK_CLERR (errcode);
    }
}

static void
ufo_resources_init (UfoResources *self)
{
    UfoResourcesPrivate *priv;

    self->priv = priv = UFO_RESOURCES_GET_PRIVATE (self);

    priv->config = NULL;
    priv->programs = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            g_free, (GDestroyNotify) release_program);
    priv->kernels = NULL;
    priv->build_opts = g_string_new ("-cl-mad-enable ");
    priv->include_paths = g_list_append (NULL, g_strdup ("."));

    priv->kernel_paths = g_list_append (NULL, g_strdup ("."));
    priv->kernel_paths = g_list_append (priv->kernel_paths, g_strdup (UFO_PLUGIN_DIR));

    initialize_opencl (priv);
}
