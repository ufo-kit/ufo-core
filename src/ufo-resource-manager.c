/**
 * SECTION:ufo-resource-manager
 * @Short_description: Manage OpenCL resources
 * @Title: UfoResourceManager
 */

#include <glib.h>
#include <stdio.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "config.h"
#include "ufo-resource-manager.h"
#include "ufo-aux.h"

G_DEFINE_TYPE(UfoResourceManager, ufo_resource_manager, G_TYPE_OBJECT)

#define UFO_RESOURCE_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_RESOURCE_MANAGER, UfoResourceManagerPrivate))

/**
 * UfoResourceManagerError:
 * @UFO_RESOURCE_MANAGER_ERROR_LOAD_PROGRAM: Could not load the OpenCL file
 * @UFO_RESOURCE_MANAGER_ERROR_CREATE_PROGRAM: Could not create a program from
 *      the sources
 * @UFO_RESOURCE_MANAGER_ERROR_BUILD_PROGRAM: Could not build program from
 *      sources
 * @UFO_RESOURCE_MANAGER_ERROR_CREATE_KERNEL: Could not create kernel
 *
 * OpenCL related errors.
 */
GQuark ufo_resource_manager_error_quark (void)
{
    return g_quark_from_static_string ("ufo-resource-manager-error-quark");
}

struct _UfoResourceManagerPrivate {
    cl_uint              num_platforms;
    cl_platform_id      *opencl_platforms;
    cl_context           opencl_context;
    cl_uint             *num_devices;       /**< Number of OpenCL devices per platform id */
    cl_device_id       **opencl_devices;    /**< Array of OpenCL devices per platform id */
    cl_command_queue    *command_queues;    /**< Array of command queues per device */

    GList       *kernel_paths;          /**< Colon-separated string with paths to kernel files */
    GHashTable  *opencl_programs;       /**< Map from filename to cl_program */
    GList       *opencl_kernels;
    GString     *opencl_build_options;
    GString     *include_paths;         /**< List of include paths "-I/foo/bar" built from added paths */

    guint current_id;                   /**< current non-assigned buffer id */
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
 * \brief Returns the error constant as a string
 * \param[in] error A valid OpenCL constant
 * \return A string containing a human-readable constant or NULL if error is
 *      invalid
 */
const gchar *
opencl_map_error (int error)
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
resource_manager_load_opencl_program (const gchar *filename)
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
resource_manager_release_kernel (gpointer data, gpointer user_data)
{
    cl_kernel kernel = (cl_kernel) data;
    CHECK_OPENCL_ERROR (clReleaseKernel (kernel));
    ufo_debug_cl ("Released kernel %p", (gpointer) kernel);
}

static void
resource_manager_release_program (gpointer data)
{
    cl_program program = (cl_program) data;
    CHECK_OPENCL_ERROR (clReleaseProgram (program));
    ufo_debug_cl ("Released program %p", (gpointer) program);
}

static gchar *
resource_manager_find_path (UfoResourceManagerPrivate *priv, const gchar *filename)
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
 * ufo_resource_manager_new:
 *
 * Create a new #UfoResourceManager instance.
 *
 * Returns: (transfer none): A new #UfoResourceManager
 */
UfoResourceManager *
ufo_resource_manager_new (void)
{
    return UFO_RESOURCE_MANAGER (g_object_new (UFO_TYPE_RESOURCE_MANAGER, NULL));
}

/**
 * ufo_resource_manager_add_paths:
 * @manager: A #UfoResourceManager
 * @paths: A string with a list of colon-separated paths
 *
 * Each path in @paths is used when searching for kernel files using
 * ufo_resource_manager_get_kernel() in the order that they are passed in.
 */
void
ufo_resource_manager_add_paths (UfoResourceManager *manager, const gchar *paths)
{
    g_return_if_fail (UFO_IS_RESOURCE_MANAGER (manager));
    UfoResourceManagerPrivate *priv = manager->priv;

    if (paths == NULL)
        return;

    /*
     * Add each of the path to the include paths for header file inclusion
     */
    gchar **path_list = g_strsplit (paths, ":", 0);
    gchar **p = path_list;

    while (*p != NULL) {
        priv->kernel_paths = g_list_append (priv->kernel_paths, g_strdup (*p));
        g_string_append_printf (priv->include_paths, "-I%s ", *(p++));
    }

    g_strfreev (path_list);
}

static cl_program
add_program_from_source (UfoResourceManagerPrivate *priv,
                         const gchar *source,
                         const gchar *options,
                         GError **error)
{
    cl_program program;
    gchar *build_options = NULL;
    cl_int errcode = CL_SUCCESS;

    program = clCreateProgramWithSource (priv->opencl_context,
                                         1, &source, NULL, &errcode);
    ufo_debug_cl ("Created program %p with error code %i", (gpointer) program, errcode);

    if (errcode != CL_SUCCESS) {
        g_set_error (error,
                     UFO_RESOURCE_MANAGER_ERROR,
                     UFO_RESOURCE_MANAGER_ERROR_CREATE_PROGRAM,
                     "Failed to create OpenCL program: %s", opencl_map_error (errcode));
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

    ufo_debug_cl ("Build program %p with error code %i", (gpointer) program, errcode);
    g_free (build_options);

    if (errcode != CL_SUCCESS) {
        g_set_error (error,
                     UFO_RESOURCE_MANAGER_ERROR,
                     UFO_RESOURCE_MANAGER_ERROR_BUILD_PROGRAM,
                     "Failed to build OpenCL program: %s", opencl_map_error (errcode));

        const gsize LOG_SIZE = 4096;
        gchar *log = (gchar *) g_malloc0(LOG_SIZE * sizeof (char));
        CHECK_OPENCL_ERROR (clGetProgramBuildInfo (program,
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
resource_manager_add_program (UfoResourceManager *manager, const gchar *filename, const gchar *options, GError **error)
{
    g_return_val_if_fail (UFO_IS_RESOURCE_MANAGER (manager) || (filename != NULL), FALSE);
    UfoResourceManagerPrivate *priv = manager->priv;

    /* Programs might be added multiple times if this is not locked */
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
    g_static_mutex_lock (&mutex);

    /* Don't process the kernel file again, if already load */
    cl_program program = g_hash_table_lookup (priv->opencl_programs, filename);

    if (program != NULL) {
        g_static_mutex_unlock (&mutex);
        return program;
    }

    gchar *path = resource_manager_find_path (priv, filename);

    if (path == NULL) {
        g_set_error (error,
                     UFO_RESOURCE_MANAGER_ERROR,
                     UFO_RESOURCE_MANAGER_ERROR_LOAD_PROGRAM,
                     "Could not find %s", filename);
        g_static_mutex_unlock (&mutex);
        return NULL;
    }

    gchar *buffer = resource_manager_load_opencl_program (path);
    g_free (path);

    if (buffer == NULL) {
        g_set_error (error,
                     UFO_RESOURCE_MANAGER_ERROR,
                     UFO_RESOURCE_MANAGER_ERROR_LOAD_PROGRAM,
                     "Could not open %s", filename);
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
resource_manager_get_kernel (UfoResourceManagerPrivate *priv, cl_program program, const gchar *kernel_name, GError **error)
{
    cl_int errcode = CL_SUCCESS;
    cl_kernel kernel = clCreateKernel (program, kernel_name, &errcode);

    ufo_debug_cl ("Created kernel `%s` from program %p", kernel_name, (gpointer) program);

    if (kernel == NULL || errcode != CL_SUCCESS) {
        g_set_error (error,
                     UFO_RESOURCE_MANAGER_ERROR,
                     UFO_RESOURCE_MANAGER_ERROR_CREATE_KERNEL,
                     "Failed to create kernel `%s`: %s", kernel_name, opencl_map_error (errcode));
        return NULL;
    }

    priv->opencl_kernels = g_list_append (priv->opencl_kernels, kernel);
    CHECK_OPENCL_ERROR (clRetainKernel (kernel));
    return kernel;
}

/**
 * ufo_resource_manager_get_kernel: (skip)
 * @manager: A #UfoResourceManager
 * @filename: Name of the .cl kernel file
 * @kernel_name: Name of a kernel
 * @error: Return location for a GError from #UfoResourceManagerError, or NULL
 *
 * Loads a and builds a kernel from a file. The file is searched in the current
 * working directory and all paths added through
 * ufo_resource_manager_add_paths ().
 *
 * Returns: a cl_kernel object that is load from @filename or %NULL on error
 */
gpointer
ufo_resource_manager_get_kernel (UfoResourceManager *manager, const gchar *filename, const gchar *kernel_name, GError **error)
{
    g_return_val_if_fail (UFO_IS_RESOURCE_MANAGER (manager) &&
            (filename != NULL) && (kernel_name != NULL), NULL);

    UfoResourceManagerPrivate *priv = UFO_RESOURCE_MANAGER_GET_PRIVATE (manager);
    GError *tmp_error = NULL;

    cl_program program = resource_manager_add_program (manager, filename, "", &tmp_error);

    if (program == NULL) {
        g_propagate_error (error, tmp_error);
        return NULL;
    }

    return resource_manager_get_kernel (priv, program, kernel_name, error);
}

/**
 * ufo_resource_manager_get_kernel_from_source: (skip)
 * @manager: A #UfoResourceManager
 * @source: OpenCL source string
 * @kernel_name: Name of a kernel
 * @error: Return location for a GError from #UfoResourceManagerError, or NULL
 *
 * Loads and builds a kernel from a string.
 *
 * Returns: a cl_kernel object that is load from @filename
 */
gpointer
ufo_resource_manager_get_kernel_from_source (UfoResourceManager *manager, const gchar *source, const gchar *kernel_name, GError **error)
{
    g_return_val_if_fail (UFO_IS_RESOURCE_MANAGER (manager) && (source != NULL) && (kernel_name != NULL), NULL);
    UfoResourceManagerPrivate *priv = UFO_RESOURCE_MANAGER_GET_PRIVATE (manager);
    cl_program program = add_program_from_source (priv, source, NULL, error);

    /*
     * We add the program under a fake file name. This looks very brittle to me
     * (kernel name could be the same as a source filename) but it should work
     * in most cases.
     */
    if (program != NULL)
        g_hash_table_insert (priv->opencl_programs, g_strdup (kernel_name), program);
    else
        return NULL;

    return resource_manager_get_kernel (priv, program, kernel_name, error);
}

/* void ufo_resource_manager_call(UfoResourceManager *manager, */
/*                                const gchar *kernel_name, */
/*                                void *command_queue, */
/*                                uint32_t work_dim, */
/*                                size_t *global_work_size, */
/*                                size_t *local_work_size, */
/*                                ...) */
/* { */
/*     cl_kernel kernel = (cl_kernel) g_hash_table_lookup(manager->priv->opencl_kernels, kernel_name); */

/*     if (kernel == NULL) */
/*         return; */

/*     cl_uint num_args = 0; */
/*     clGetKernelInfo(kernel, CL_KERNEL_NUM_ARGS, sizeof(cl_uint), &num_args, NULL); */
/*     void *ptr = NULL; */
/*     size_t size = 0; */
/*     va_list ap; */
/*     va_start(ap, local_work_size); */
/*     g_message("parsing arguments"); */

/*     for (guint i = 0; i < num_args; i++) { */
/*         size = va_arg(ap, size_t); */
/*         ptr = va_arg(ap, void *); */
/*         clSetKernelArg(kernel, i, size, ptr); */
/*     } */

/*     va_end(ap); */
/*     clEnqueueNDRangeKernel(command_queue, kernel, */
/*                            work_dim, NULL, global_work_size, local_work_size, */
/*                            0, NULL, NULL); */
/* } */

/**
 * ufo_resource_manager_get_context: (skip)
 * @manager: A #UfoResourceManager
 *
 * Returns the OpenCL context object that is used by the resource manager. This
 * context can be used to initialize othe third-party libraries.
 *
 * Return value: A cl_context object.
 */
gpointer
ufo_resource_manager_get_context (UfoResourceManager *manager)
{
    g_return_val_if_fail (UFO_IS_RESOURCE_MANAGER (manager), NULL);
    return manager->priv->opencl_context;
}

/**
 * ufo_resource_manager_request_buffer: (skip)
 * @manager: A #UfoResourceManager
 * @num_dims: (in): Number of dimensions
 * @dim_size: (in) (array): Size of each dimension
 * @data: (in) (allow-none): Data used to initialize the buffer with, or NULL
 * @command_queue: (in) (allow-none): If data should be copied onto the device,
 * a cl_command_queue must be provide, or NULL
 *
 * Creates a new #UfoBuffer and initializes it with data on demand. If
 * non-floating point data have to be uploaded, use ufo_buffer_set_host_array()
 * and ufo_buffer_reinterpret() on the #UfoBuffer.
 *
 * Return value: A new #UfoBuffer with the given dimensions
 */
UfoBuffer *
ufo_resource_manager_request_buffer (UfoResourceManager *manager,
                                     guint               num_dims,
                                     const guint        *dim_size,
                                     gfloat             *data,
                                     gpointer            command_queue)
{
    g_return_val_if_fail (UFO_IS_RESOURCE_MANAGER (manager), NULL);
    g_return_val_if_fail ((num_dims > UFO_BUFFER_MAX_NDIMS) || (dim_size != NULL), NULL);

    UfoResourceManagerPrivate *priv = UFO_RESOURCE_MANAGER_GET_PRIVATE (manager);
    UfoBuffer *buffer = ufo_buffer_new (num_dims, dim_size);
    const gsize num_bytes = ufo_buffer_get_size (buffer);
    cl_mem_flags mem_flags = CL_MEM_READ_WRITE;

    if ((data != NULL) && (command_queue != NULL))
        mem_flags |= CL_MEM_COPY_HOST_PTR;

    cl_int errcode;
    cl_mem buffer_mem = clCreateBuffer (priv->opencl_context,
                                        mem_flags,
                                        num_bytes,
                                        data, &errcode);
    CHECK_OPENCL_ERROR (errcode);
    ufo_buffer_set_cl_mem (buffer, buffer_mem);

    if ((data) && (command_queue == NULL))
        ufo_buffer_set_host_array (buffer, data, num_bytes, NULL);

    return buffer;
}

/**
 * ufo_resource_manager_memalloc: (skip)
 * @manager: A #UfoResourceManager
 * @size: Size of cl_mem in bytes
 *
 * Allocates a new cl_mem object with the given size.
 *
 * Return value: A cl_mem object
 */
gpointer
ufo_resource_manager_memalloc (UfoResourceManager *manager, gsize size)
{
    g_return_val_if_fail (UFO_IS_RESOURCE_MANAGER (manager) || (size == 0), NULL);
    cl_int errcode = CL_SUCCESS;
    cl_mem mem = clCreateBuffer (manager->priv->opencl_context, CL_MEM_READ_WRITE, size, NULL, &errcode);
    CHECK_OPENCL_ERROR (errcode);
    return mem;
}

/**
 * ufo_resource_manager_memdup: (skip)
 * @manager: A #UfoResourceManager
 * @memobj: A cl_mem object
 *
 * Creates a new cl_mem object with the same size and content as a given cl_mem
 * object.
 *
 * Return value: A new cl_mem object
 */
gpointer
ufo_resource_manager_memdup (UfoResourceManager *manager, gpointer memobj)
{
    g_return_val_if_fail (UFO_IS_RESOURCE_MANAGER (manager) || (memobj == NULL), NULL);
    UfoResourceManagerPrivate *priv = UFO_RESOURCE_MANAGER_GET_PRIVATE (manager);
    cl_int errcode;
    cl_event event;
    cl_mem mem, dup;
    size_t size;

    mem = (cl_mem) memobj;
    CHECK_OPENCL_ERROR (clGetMemObjectInfo (mem, CL_MEM_SIZE, sizeof (size_t), &size, NULL));
    dup = clCreateBuffer (priv->opencl_context, CL_MEM_READ_WRITE, size, NULL, &errcode);
    CHECK_OPENCL_ERROR (errcode);

    CHECK_OPENCL_ERROR (clEnqueueCopyBuffer (priv->command_queues[0],
                mem, dup, 0, 0, size, 0, NULL, &event));

    CHECK_OPENCL_ERROR (clWaitForEvents (1, &event));
    CHECK_OPENCL_ERROR (clReleaseEvent (event));

    return dup;
}

guint ufo_resource_manager_get_new_id (UfoResourceManager *manager)
{
    g_return_val_if_fail (UFO_IS_RESOURCE_MANAGER (manager), 0);
    guint id;
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
    g_static_mutex_lock (&mutex);
    id = manager->priv->current_id++;
    g_static_mutex_unlock (&mutex);
    return id;
}

/**
 * ufo_resource_manager_get_command_queues:
 * @manager: A #UfoResourceManager
 * @command_queues: (out): Sets pointer to command_queues array
 * @num_queues: (out): Number of queues
 *
 * Return the number and actual command queues.
 */
void ufo_resource_manager_get_command_queues (UfoResourceManager *manager, gpointer *command_queues, guint *num_queues)
{
    g_return_if_fail (UFO_IS_RESOURCE_MANAGER (manager) || (command_queues != NULL) || (num_queues != NULL));
    /* FIXME: Use only first platform */
    *num_queues = manager->priv->num_devices[0];
    *command_queues = manager->priv->command_queues;
}

/**
 * ufo_resource_manager_get_command_queue: (skip)
 * @manager: A #UfoResourceManager
 * @queue: The number of the queue which must be less than the number returned
 *      by ufo_resource_manager_get_number_of_devices ().
 *
 * Return a specific command queue.
 *
 * Return value: The ith cl_command_queue
 * Since: 0.2
 */
gpointer
ufo_resource_manager_get_command_queue (UfoResourceManager *manager, guint queue)
{
    g_return_val_if_fail (UFO_IS_RESOURCE_MANAGER (manager), NULL);
    g_return_val_if_fail (queue >= manager->priv->num_devices[0], NULL);
    return manager->priv->command_queues[queue];
}

gint
ufo_resource_manager_get_queue_number (UfoResourceManager *manager,
                                       gpointer            command_queue)
{
    UfoResourceManagerPrivate *priv;

    g_return_val_if_fail (UFO_IS_RESOURCE_MANAGER (manager), -1);
    priv = UFO_RESOURCE_MANAGER_GET_PRIVATE (manager);

    for (guint i = 0; i < priv->num_devices[0]; i++)
        if (priv->command_queues[i] == command_queue)
            return (gint) i;

    return -1;
}

/**
 * ufo_resource_manager_get_number_of_devices:
 * @manager: A #UfoResourceManager
 *
 * Get number of acceleration devices such as GPUs.
 *
 * Return value: Number of acceleration devices.
 */
guint ufo_resource_manager_get_number_of_devices (UfoResourceManager *manager)
{
    g_return_val_if_fail (UFO_IS_RESOURCE_MANAGER (manager), 0);
    return manager->priv->num_devices[0];
}

static void ufo_resource_manager_finalize (GObject *gobject)
{
    UfoResourceManager *self = UFO_RESOURCE_MANAGER (gobject);
    UfoResourceManagerPrivate *priv = UFO_RESOURCE_MANAGER_GET_PRIVATE (self);

    g_hash_table_destroy (priv->opencl_programs);
    g_list_foreach (priv->kernel_paths, (GFunc) g_free, NULL);
    g_list_free (priv->kernel_paths);
    g_list_foreach (priv->opencl_kernels, resource_manager_release_kernel, NULL);
    g_list_free (priv->opencl_kernels);

    for (guint i = 0; i < priv->num_devices[0]; i++)
        CHECK_OPENCL_ERROR (clReleaseCommandQueue (priv->command_queues[i]));

    CHECK_OPENCL_ERROR (clReleaseContext (priv->opencl_context));
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

    G_OBJECT_CLASS (ufo_resource_manager_parent_class)->finalize (gobject);
    g_message ("UfoResourceManager: finalized");
}

static void
ufo_resource_manager_class_init (UfoResourceManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = ufo_resource_manager_finalize;
    g_type_class_add_private (klass, sizeof (UfoResourceManagerPrivate));
}

static void
ufo_resource_manager_init (UfoResourceManager *self)
{
    UfoResourceManagerPrivate *priv;
    self->priv = priv = UFO_RESOURCE_MANAGER_GET_PRIVATE (self);
    priv->current_id = 0;

    priv->opencl_programs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, resource_manager_release_program);
    priv->kernel_paths = g_list_append (NULL, g_strdup ("."));
    priv->opencl_kernels = NULL;
    priv->opencl_platforms = NULL;
    priv->opencl_build_options = g_string_new ("-cl-mad-enable ");
    priv->include_paths = g_string_new ("-I. ");

    g_message ("Ufo version %s\n", UFO_VERSION);

    /* initialize OpenCL subsystem */
    int errcode = CL_SUCCESS;
    CHECK_OPENCL_ERROR (clGetPlatformIDs (0, NULL, &priv->num_platforms));
    priv->opencl_platforms = g_malloc0(priv->num_platforms * sizeof (cl_platform_id));

    CHECK_OPENCL_ERROR (clGetPlatformIDs (priv->num_platforms, priv->opencl_platforms, NULL));
    priv->num_devices = g_malloc0(priv->num_platforms * sizeof (cl_uint));
    priv->opencl_devices = g_malloc0(priv->num_platforms * sizeof (cl_device_id *));

    ufo_debug_cl ("Number of OpenCL platforms: %i", priv->num_platforms);

    /* Get devices for each available platform */
    gchar *info_buffer = g_malloc0(256);

    for (guint i = 0; i < priv->num_platforms; i++) {
        cl_uint num_devices;
        cl_platform_id platform = priv->opencl_platforms[i];

        CHECK_OPENCL_ERROR (clGetPlatformInfo (platform, CL_PLATFORM_NAME, 256, info_buffer, NULL));
        ufo_debug_cl ("--- %s ---", info_buffer);
        CHECK_OPENCL_ERROR (clGetPlatformInfo (platform, CL_PLATFORM_VENDOR, 256, info_buffer, NULL));
        ufo_debug_cl (" Vendor        : %s", info_buffer);

        if (g_str_has_prefix (info_buffer, "NVIDIA"))
            g_string_append (priv->opencl_build_options, "-cl-nv-verbose -DVENDOR=NVIDIA");
        else if (g_str_has_prefix (info_buffer, "Advanced Micro Devices"))
            g_string_append (priv->opencl_build_options, "-DVENDOR=AMD");

        CHECK_OPENCL_ERROR (clGetPlatformInfo (platform, CL_PLATFORM_VERSION, 256, info_buffer, NULL));
        ufo_debug_cl (" Version       : %s", info_buffer);

        CHECK_OPENCL_ERROR (clGetDeviceIDs (platform,
                                   CL_DEVICE_TYPE_ALL,
                                   0, NULL,
                                   &num_devices));
        priv->opencl_devices[i] = g_malloc0(num_devices * sizeof (cl_device_id));

        CHECK_OPENCL_ERROR (clGetDeviceIDs (platform,
                                   CL_DEVICE_TYPE_ALL,
                                   num_devices, priv->opencl_devices[i],
                                   NULL));
        priv->num_devices[i] = num_devices;

        ufo_debug_cl (" Device count  : %i", num_devices);
        ufo_debug_cl (" Build options : %s", priv->opencl_build_options->str);
    }

    g_free (info_buffer);
    cl_command_queue_properties queue_properties = CL_QUEUE_PROFILING_ENABLE;

    /* XXX: create context for each platform?! */
    if (priv->num_platforms > 0) {
        priv->opencl_context = clCreateContext (NULL,
                                                priv->num_devices[0],
                                                priv->opencl_devices[0],
                                                NULL, NULL, &errcode);
        CHECK_OPENCL_ERROR (errcode);
        priv->command_queues = g_malloc0(priv->num_devices[0] * sizeof (cl_command_queue));

        for (guint i = 0; i < priv->num_devices[0]; i++) {
            priv->command_queues[i] = clCreateCommandQueue (priv->opencl_context,
                                      priv->opencl_devices[0][i],
                                      queue_properties, &errcode);
            CHECK_OPENCL_ERROR (errcode);
        }
    }
}
