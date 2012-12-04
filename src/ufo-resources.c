/**
 * SECTION:ufo-resources
 * @Short_description: Manage OpenCL resources
 * @Title: UfoResources
 *
 * The #UfoResources creates the OpenCL environment and loads OpenCL
 * kernels from text files.
 */

#include <glib.h>
#include <stdio.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "ufo-resources.h"
#include "ufo-configurable.h"

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

    cl_uint          num_platforms;
    cl_platform_id  *opencl_platforms;
    cl_context       opencl_context;
    cl_uint         *num_devices;       /**< Number of OpenCL devices per platform id */
    cl_device_id   **opencl_devices;    /**< Array of OpenCL devices per platform id */
    cl_command_queue *command_queues;    /**< Array of command queues per device */

    GList       *kernel_paths;          /**< Colon-separated string with paths to kernel files */
    GHashTable  *opencl_programs;       /**< Map from filename to cl_program */
    GList       *opencl_kernels;
    GString     *opencl_build_options;
    GString     *include_paths;         /**< List of include paths "-I/foo/bar" built from added paths */
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
resources_load_opencl_program (const gchar *filename)
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
resources_release_kernel (gpointer data, gpointer user_data)
{
    cl_kernel kernel = (cl_kernel) data;
    UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (kernel));
}

static void
resources_release_program (gpointer data)
{
    cl_program program = (cl_program) data;
    UFO_RESOURCES_CHECK_CLERR (clReleaseProgram (program));
}

static gchar *
resources_find_path (UfoResourcesPrivate *priv, const gchar *filename)
{
    /* Check first if filename is already a path */
    if (g_path_is_absolute (filename)) {
        if (g_file_test (filename, G_FILE_TEST_EXISTS))
            return g_strdup (filename);
        else
            return NULL;
    }

    /* If it is not a path, search in all paths that were added */
    GList *elem = g_list_first (priv->kernel_paths);

    while (elem != NULL) {
        gchar *path = g_strdup_printf ("%s%c%s", (gchar *) elem->data, G_DIR_SEPARATOR, filename);

        if (g_file_test (path, G_FILE_TEST_EXISTS))
            return path;

        g_free (path);
        elem = g_list_next (elem);
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
                                        "configuration", config,
                                        NULL));
}

static void
add_paths (UfoResourcesPrivate *priv, gchar *paths[])
{
    if (paths == NULL)
        return;

    for (guint i = 0; paths[i] != NULL; i++) {
        priv->kernel_paths = g_list_append (priv->kernel_paths, g_strdup (paths[i]));
        g_string_append_printf (priv->include_paths, "-I%s ", paths[i]);
    }
}

static cl_program
add_program_from_source (UfoResourcesPrivate *priv,
                         const gchar *source,
                         const gchar *options,
                         GError **error)
{
    cl_program program;
    gchar *build_options = NULL;
    cl_int errcode = CL_SUCCESS;

    program = clCreateProgramWithSource (priv->opencl_context,
                                         1, &source, NULL, &errcode);

    if (errcode != CL_SUCCESS) {
        g_set_error (error,
                     UFO_RESOURCES_ERROR,
                     UFO_RESOURCES_ERROR_CREATE_PROGRAM,
                     "Failed to create OpenCL program: %s", ufo_resources_clerr (errcode));
        return NULL;
    }

    if (options != NULL)
        build_options = g_strdup_printf ("%s %s %s", priv->opencl_build_options->str, priv->include_paths->str, options);
    else
        build_options = g_strdup_printf ("%s %s", priv->opencl_build_options->str, priv->include_paths->str);

    errcode = clBuildProgram (program,
                              priv->num_devices[0],
                              priv->opencl_devices[0],
                              build_options,
                              NULL, NULL);

    g_free (build_options);

    if (errcode != CL_SUCCESS) {
        g_set_error (error,
                     UFO_RESOURCES_ERROR,
                     UFO_RESOURCES_ERROR_BUILD_PROGRAM,
                     "Failed to build OpenCL program: %s", ufo_resources_clerr (errcode));

        const gsize LOG_SIZE = 4096;
        gchar *log = (gchar *) g_malloc0(LOG_SIZE * sizeof (char));
        UFO_RESOURCES_CHECK_CLERR (clGetProgramBuildInfo (program,
                                                   priv->opencl_devices[0][0],
                                                   CL_PROGRAM_BUILD_LOG,
                                                   LOG_SIZE, (void *) log, NULL));
        g_print ("\n=== Build log for s===%s\n\n", log);
        g_free (log);
        return NULL;
    }

    return program;
}

static cl_program
resources_add_program (UfoResources *resources, const gchar *filename, const gchar *options, GError **error)
{
    g_return_val_if_fail (UFO_IS_RESOURCES (resources) || (filename != NULL), FALSE);
    UfoResourcesPrivate *priv = resources->priv;

    /* Programs might be added multiple times if this is not locked */
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
    g_static_mutex_lock (&mutex);

    /* Don't process the kernel file again, if already load */
    cl_program program = g_hash_table_lookup (priv->opencl_programs, filename);

    if (program != NULL) {
        g_static_mutex_unlock (&mutex);
        return program;
    }

    gchar *path = resources_find_path (priv, filename);

    if (path == NULL) {
        g_set_error (error,
                     UFO_RESOURCES_ERROR,
                     UFO_RESOURCES_ERROR_LOAD_PROGRAM,
                     "Could not find `%s'. Maybe you forgot to pass a configuration?", filename);
        g_static_mutex_unlock (&mutex);
        return NULL;
    }

    gchar *buffer = resources_load_opencl_program (path);
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
        g_hash_table_insert (priv->opencl_programs, g_strdup (filename), program);

    g_static_mutex_unlock (&mutex);
    g_free (buffer);
    return program;
}

static cl_kernel
resources_get_kernel (UfoResourcesPrivate *priv,
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

    priv->opencl_kernels = g_list_append (priv->opencl_kernels, kernel);
    UFO_RESOURCES_CHECK_CLERR (clRetainKernel (kernel));
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
 * Returns: a cl_kernel object that is load from @filename or %NULL on error
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
    program = resources_add_program (resources, filename, "", &tmp_error);

    if (program == NULL) {
        g_propagate_error (error, tmp_error);
        return NULL;
    }

    return resources_get_kernel (priv, program, kernel, error);
}

/**
 * ufo_resources_get_kernel_from_source: (skip)
 * @resources: A #UfoResources
 * @source: OpenCL source string
 * @kernel_name: Name of a kernel
 * @error: Return location for a GError from #UfoResourcesError, or NULL
 *
 * Loads and builds a kernel from a string.
 *
 * Returns: a cl_kernel object that is load from @filename
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
        g_hash_table_insert (priv->opencl_programs, g_strdup (kernel), program);
    else
        return NULL;

    return resources_get_kernel (priv, program, kernel, error);
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
    return resources->priv->opencl_context;
}

/**
 * ufo_resources_get_cmd_queues:
 * @resources: A #UfoResources
 *
 * Returns: (element-type gpointer) (transfer container): List with
 * cl_command_queue objects.
 */
GList *
ufo_resources_get_cmd_queues (UfoResources *resources)
{
    UfoResourcesPrivate *priv;
    GList *result = NULL;

    g_return_val_if_fail (UFO_IS_RESOURCES (resources), NULL);
    priv = resources->priv;

    for (guint i = 0; i < priv->num_devices[0]; i++)
        result = g_list_append (result, priv->command_queues[i]);

    return result;
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
                UfoConfig *config;
                gchar **paths;
                GObject *value_object = g_value_get_object (value);

                if (priv->config)
                    g_object_unref (priv->config);


                if (value_object != NULL) {
                    config = UFO_CONFIG (value_object);
                    paths = ufo_config_get_paths (config);
                    add_paths (priv, paths);
                    g_strfreev (paths);
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
ufo_resources_finalize (GObject *object)
{
    UfoResourcesPrivate *priv = UFO_RESOURCES_GET_PRIVATE (object);

    g_hash_table_destroy (priv->opencl_programs);
    g_list_foreach (priv->kernel_paths, (GFunc) g_free, NULL);
    g_list_free (priv->kernel_paths);
    g_list_foreach (priv->opencl_kernels, resources_release_kernel, NULL);
    g_list_free (priv->opencl_kernels);

    for (guint i = 0; i < priv->num_devices[0]; i++)
        UFO_RESOURCES_CHECK_CLERR (clReleaseCommandQueue (priv->command_queues[i]));

    UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->opencl_context));

    g_string_free (priv->opencl_build_options, TRUE);
    g_string_free (priv->include_paths, TRUE);

    for (guint i = 0; i < priv->num_platforms; i ++)
        g_free (priv->opencl_devices[i]);

    g_free (priv->num_devices);
    g_free (priv->opencl_devices);
    g_free (priv->opencl_platforms);
    g_free (priv->command_queues);

    priv->num_devices = NULL;
    priv->opencl_kernels = NULL;
    priv->opencl_devices = NULL;
    priv->opencl_platforms = NULL;

    G_OBJECT_CLASS (ufo_resources_parent_class)->finalize (object);
    g_message ("UfoResources: finalized");
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

static void
ufo_resources_init (UfoResources *self)
{
    UfoResourcesPrivate *priv;
    self->priv = priv = UFO_RESOURCES_GET_PRIVATE (self);

    priv->config = NULL;
    priv->opencl_programs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, resources_release_program);
    priv->kernel_paths = g_list_append (NULL, g_strdup ("."));
    priv->opencl_kernels = NULL;
    priv->opencl_platforms = NULL;
    priv->opencl_build_options = g_string_new ("-cl-mad-enable ");
    priv->include_paths = g_string_new ("-I. ");

    /* initialize OpenCL subsystem */
    int errcode = CL_SUCCESS;
    UFO_RESOURCES_CHECK_CLERR (clGetPlatformIDs (0, NULL, &priv->num_platforms));
    priv->opencl_platforms = g_malloc0(priv->num_platforms * sizeof (cl_platform_id));

    UFO_RESOURCES_CHECK_CLERR (clGetPlatformIDs (priv->num_platforms, priv->opencl_platforms, NULL));
    priv->num_devices = g_malloc0(priv->num_platforms * sizeof (cl_uint));
    priv->opencl_devices = g_malloc0(priv->num_platforms * sizeof (cl_device_id *));

    /* Get devices for each available platform */
    gchar *info_buffer = g_malloc0 (256);

    for (guint i = 0; i < priv->num_platforms; i++) {
        cl_uint num_devices;
        cl_platform_id platform = priv->opencl_platforms[i];

        UFO_RESOURCES_CHECK_CLERR (clGetPlatformInfo (platform, CL_PLATFORM_VENDOR, 256, info_buffer, NULL));

        if (g_str_has_prefix (info_buffer, "NVIDIA"))
            g_string_append (priv->opencl_build_options, "-cl-nv-verbose -DVENDOR=NVIDIA");
        else if (g_str_has_prefix (info_buffer, "Advanced Micro Devices"))
            g_string_append (priv->opencl_build_options, "-DVENDOR=AMD");

        UFO_RESOURCES_CHECK_CLERR (clGetDeviceIDs (platform,
                                                   CL_DEVICE_TYPE_ALL,
                                                   0, NULL,
                                                   &num_devices));
        priv->opencl_devices[i] = g_malloc0 (num_devices * sizeof (cl_device_id));

        UFO_RESOURCES_CHECK_CLERR (clGetDeviceIDs (platform,
                                                   CL_DEVICE_TYPE_ALL,
                                                   num_devices, priv->opencl_devices[i],
                                                   NULL));
        priv->num_devices[i] = num_devices;
    }

    g_free (info_buffer);
    cl_command_queue_properties queue_properties = CL_QUEUE_PROFILING_ENABLE;

    /* XXX: create context for each platform?! */
    if (priv->num_platforms > 0) {
        priv->opencl_context = clCreateContext (NULL,
                                                priv->num_devices[0],
                                                priv->opencl_devices[0],
                                                NULL, NULL, &errcode);
        UFO_RESOURCES_CHECK_CLERR (errcode);
        priv->command_queues = g_malloc0 (priv->num_devices[0] * sizeof (cl_command_queue));

        for (guint i = 0; i < priv->num_devices[0]; i++) {
            priv->command_queues[i] = clCreateCommandQueue (priv->opencl_context,
                                      priv->opencl_devices[0][i],
                                      queue_properties, &errcode);
            UFO_RESOURCES_CHECK_CLERR (errcode);
        }
    }
}
