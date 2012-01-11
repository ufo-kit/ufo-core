#include <glib.h>
#include <stdio.h>

#include "config.h"
#include "ufo-resource-manager.h"

G_DEFINE_TYPE(UfoResourceManager, ufo_resource_manager, G_TYPE_OBJECT);

#define UFO_RESOURCE_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_RESOURCE_MANAGER, UfoResourceManagerPrivate))

#define UFO_RESOURCE_MANAGER_ERROR ufo_resource_manager_error_quark()
enum UfoResourceManagerError {
    UFO_RESOURCE_MANAGER_ERROR_LOAD_PROGRAM,
    UFO_RESOURCE_MANAGER_ERROR_CREATE_PROGRAM,
    UFO_RESOURCE_MANAGER_ERROR_BUILD_PROGRAM,
    UFO_RESOURCE_MANAGER_ERROR_KERNEL_NOT_FOUND
};

struct _UfoResourceManagerPrivate {
    cl_uint num_platforms;
    cl_platform_id *opencl_platforms;

    cl_context opencl_context;

    cl_uint *num_devices;               /**< Number of OpenCL devices per platform id */
    cl_device_id **opencl_devices;      /**< Array of OpenCL devices per platform id */
    cl_command_queue *command_queues;   /**< Array of command queues per device */

    gchar *paths;                       /**< Colon-separated string with paths to kernel files */
    GList *opencl_files;
    GList *opencl_kernel_table;
    GList *opencl_programs;
    GString *opencl_build_options;

    GHashTable *opencl_kernels;         /**< maps from kernel string to cl_kernel */

    guint current_id;                   /**< current non-assigned buffer id */

    float upload_time;
    float download_time;
};

const gchar* opencl_error_msgs[] = {
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
const gchar* opencl_map_error(int error)
{
    if (error >= -14)
        return opencl_error_msgs[-error];
    if (error <= -30)
        return opencl_error_msgs[-error-15];
    return NULL;
}

static gchar *resource_manager_load_opencl_program(const gchar *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
        return NULL;

    fseek(fp, 0, SEEK_END);
    const size_t length = ftell(fp);
    rewind(fp);

    gchar *buffer = (gchar *) g_malloc0(length);
    if (buffer == NULL) {
        fclose(fp);
        return NULL;
    }

    size_t buffer_length = fread(buffer, 1, length, fp);
    fclose(fp);
    if (buffer_length != length) {
        g_free(buffer);
        return NULL;
    }
    return buffer;
}

static void resource_manager_release_kernel(gpointer data)
{
    cl_kernel kernel = (cl_kernel) data;
    CHECK_ERROR(clReleaseKernel(kernel));
}

static void resource_manager_release_program(gpointer data, gpointer user_data)
{
    cl_program program = (cl_program) data;
    CHECK_ERROR(clReleaseProgram(program));
}

static gchar *resource_manager_find_path(UfoResourceManagerPrivate* priv,
        const gchar *filename)
{
    /* Check first if filename is already a path */
    if (g_path_is_absolute(filename)) {
        if (g_file_test(filename, G_FILE_TEST_EXISTS))
            return g_strdup(filename);
        else
            return NULL;
    }

    /* If it is not a path, search in all paths that were added */
    gchar **path_list = g_strsplit(priv->paths, ":", 0);
    gchar **p = path_list;
    while (*p != NULL) {
        gchar *path = g_strdup_printf("%s%c%s", *(p++), G_DIR_SEPARATOR, filename);
        if (g_file_test(path, G_FILE_TEST_EXISTS)) {
            g_strfreev(path_list);
            return path;
        }
        g_free(path);
    }

    g_strfreev(path_list);
    return NULL;
}

GQuark ufo_resource_manager_error_quark(void)
{
    return g_quark_from_static_string("ufo-resource-manager-error-quark");
}

/**
 * ufo_resource_manager: Create a new #UfoResourceManager instance.
 *
 * Return value: A new #UfoResourceManager
 */
UfoResourceManager *ufo_resource_manager()
{
    static UfoResourceManager *manager = NULL;
    if (manager == NULL)
        manager = UFO_RESOURCE_MANAGER(g_object_new(UFO_TYPE_RESOURCE_MANAGER, NULL));

    return manager; 
}

/**
 * ufo_resource_manager_add_paths:
 * @resource_manager: A #UfoResourceManager
 * @paths: A string with a list of colon-separated paths
 *
 * Each path is used when searching for kernel files using
 * ufo_resource_manager_add_program() in that order that they are passed.
 */
void ufo_resource_manager_add_paths(UfoResourceManager *resource_manager, const gchar *paths)
{
    UfoResourceManagerPrivate *priv = resource_manager->priv;
    gchar *new_paths = g_strdup_printf("%s:%s", priv->paths, paths);
    g_free(priv->paths);
    priv->paths = new_paths;
}

/**
 * ufo_resource_manager_add_program:
 * @resource_manager: A #UfoResourceManager
 * @filename: Name or path of an ASCII-encoded kernel file 
 * @options: (in) (allow-none): Additional build options such as "-DX=Y", or NULL
 * @error: Return locatation for a GError, or NULL
 *
 * Opens, loads and builds an OpenCL kernel file either by an absolute path or
 * by looking up the file in all directories specified by
 * ufo_resource_manager_add_paths(). After successfully building the program,
 * individual kernels can be accessed using ufo_resource_manager_get_kernel().
 *
 * Return value: TRUE on success, FALSE if an error occurred
 */
gboolean ufo_resource_manager_add_program(
        UfoResourceManager *resource_manager, 
        const gchar *filename, 
        const gchar *options,
        GError **error)
{
    UfoResourceManagerPrivate *priv = resource_manager->priv;

    /* programs might be added multiple times if this is not locked */
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
    g_static_mutex_lock(&mutex);

    /* Don't process the kernel file again, if already load */
    if (g_list_find_custom(priv->opencl_files, filename, (GCompareFunc) g_strcmp0)) {
        g_static_mutex_unlock(&mutex);
        return TRUE;
    }

    gchar *path = resource_manager_find_path(priv, filename);
    if (path == NULL) {
        g_set_error(error,
                UFO_RESOURCE_MANAGER_ERROR,
                UFO_RESOURCE_MANAGER_ERROR_LOAD_PROGRAM,
                "Could not find %s",
                filename);
        g_static_mutex_unlock(&mutex);
        return FALSE;
    }

    gchar *buffer = resource_manager_load_opencl_program(path);
    g_free(path);
    if (buffer == NULL) {
        g_set_error(error,
                UFO_RESOURCE_MANAGER_ERROR,
                UFO_RESOURCE_MANAGER_ERROR_LOAD_PROGRAM,
                "Could not open %s",
                filename);
        g_static_mutex_unlock(&mutex);
        return FALSE;
    }

    priv->opencl_files = g_list_append(priv->opencl_files, g_strdup(filename));

    int errcode = CL_SUCCESS;
    cl_program program = clCreateProgramWithSource(priv->opencl_context,
            1, (const char **) &buffer, NULL, &errcode);

    if (errcode != CL_SUCCESS) {
        g_set_error(error,
                UFO_RESOURCE_MANAGER_ERROR,
                UFO_RESOURCE_MANAGER_ERROR_CREATE_PROGRAM,
                "Failed to create OpenCL program: %s", opencl_map_error(errcode));
        g_static_mutex_unlock(&mutex);
        g_free(buffer);
        return FALSE;
    }
    priv->opencl_programs = g_list_append(priv->opencl_programs, program);

    /* Concatenate global build options with per-program options */
    gchar *build_options = NULL;
    if (options != NULL)
        build_options = g_strdup_printf("%s %s", priv->opencl_build_options->str, options);
    else
        build_options = priv->opencl_build_options->str;

    /* TODO: build program for each platform?!*/
    errcode = clBuildProgram(program, 
            priv->num_devices[0], 
            priv->opencl_devices[0],
            build_options,
            NULL, NULL);

    if (options)
        g_free(build_options);

    if (errcode != CL_SUCCESS) {
        g_set_error(error,
                UFO_RESOURCE_MANAGER_ERROR,
                UFO_RESOURCE_MANAGER_ERROR_BUILD_PROGRAM,
                "Failed to build OpenCL program");

        const int LOG_SIZE = 4096;
        gchar* log = (gchar *) g_malloc0(LOG_SIZE * sizeof(char));
        CHECK_ERROR(clGetProgramBuildInfo(program, priv->opencl_devices[0][0], 
            CL_PROGRAM_BUILD_LOG, LOG_SIZE, (void*) log, NULL));

        g_print("\n=== Build log for %s===%s\n\n", filename, log);
        g_free(log);
        g_free(buffer);
        g_static_mutex_unlock(&mutex);
        return FALSE;
    }

    /* Create all kernels in the program source and map their function names to
     * the corresponding cl_kernel object */
    cl_uint num_kernels;
    CHECK_ERROR(clCreateKernelsInProgram(program, 0, NULL, &num_kernels));
    cl_kernel *kernels = (cl_kernel *) g_malloc0(num_kernels * sizeof(cl_kernel));
    CHECK_ERROR(clCreateKernelsInProgram(program, num_kernels, kernels, NULL));
    priv->opencl_kernel_table = g_list_append(priv->opencl_kernel_table, kernels);

    for (guint i = 0; i < num_kernels; i++) {
        size_t kernel_name_length;    
        CHECK_ERROR(clGetKernelInfo(kernels[i], CL_KERNEL_FUNCTION_NAME, 
                0, NULL, 
                &kernel_name_length));

        gchar *kernel_name = (gchar *) g_malloc0(kernel_name_length);
        CHECK_ERROR(clGetKernelInfo(kernels[i], CL_KERNEL_FUNCTION_NAME, 
                        kernel_name_length, kernel_name, 
                        NULL));

        g_hash_table_insert(priv->opencl_kernels, kernel_name, kernels[i]);
    }

    g_static_mutex_unlock(&mutex);
    g_free(buffer);
    return TRUE;
}

/**
 * ufo_resource_manager_get_kernel:
 * @resource_manager: A #UfoResourceManager
 * @kernel_name: Name of a kernel 
 * @error: Return location for a GError, or NULL
 *
 * Returns a kernel that has been previously loaded with ufo_resource_manager_add_program()
 *
 * Return value: The cl_kernel object identified by the kernel name
 */
gpointer ufo_resource_manager_get_kernel(UfoResourceManager *resource_manager, const gchar *kernel_name, GError **error)
{
    UfoResourceManager *self = resource_manager;
    cl_kernel kernel = (cl_kernel) g_hash_table_lookup(self->priv->opencl_kernels, kernel_name);
    if (kernel == NULL) {
        g_set_error(error,
                UFO_RESOURCE_MANAGER_ERROR,
                UFO_RESOURCE_MANAGER_ERROR_KERNEL_NOT_FOUND,
                "Kernel '%s' not found", kernel_name);
        return NULL;
    }
    CHECK_ERROR(clRetainKernel(kernel));
    return kernel;
}

void ufo_resource_manager_call(UfoResourceManager *resource_manager, 
        const gchar *kernel_name, 
        void *command_queue,
        uint32_t work_dim,
        size_t *global_work_size,
        size_t *local_work_size,
        ...)
{
    cl_kernel kernel = (cl_kernel) g_hash_table_lookup(resource_manager->priv->opencl_kernels, kernel_name);
    if (kernel == NULL)
        return;

    cl_uint num_args = 0;
    clGetKernelInfo(kernel, CL_KERNEL_NUM_ARGS, sizeof(cl_uint), &num_args, NULL);
    void *ptr = NULL;
    size_t size = 0;
    va_list ap;
    va_start(ap, local_work_size);
    g_message("parsing arguments");
    for (int i = 0; i < num_args; i++) {
        size = va_arg(ap, size_t);
        ptr = va_arg(ap, void *); 
        clSetKernelArg(kernel, i, size, ptr);
    }
    va_end(ap);
    clEnqueueNDRangeKernel(command_queue, kernel,
        work_dim, NULL, global_work_size, local_work_size,
        0, NULL, NULL);
}

/**
 * ufo_resource_manager_get_context:
 * @resource_manager: A #UfoResourceManager
 *
 * Returns the OpenCL context object that is used by the resource manager. This
 * context can be used to initialize othe third-party libraries.
 *
 * Return value: A cl_context object.
 */
gpointer ufo_resource_manager_get_context(UfoResourceManager *resource_manager)
{
    return resource_manager->priv->opencl_context; 
}

/**
 * ufo_resource_manager_request_buffer:
 * @resource_manager: A #UfoResourceManager
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
UfoBuffer *ufo_resource_manager_request_buffer(UfoResourceManager *resource_manager, 
        int num_dims, const int *dim_size, float *data, gpointer command_queue)
{
    UfoResourceManagerPrivate *priv = UFO_RESOURCE_MANAGER_GET_PRIVATE(resource_manager);
    UfoBuffer *buffer = ufo_buffer_new(num_dims, dim_size);
    const gsize num_bytes = ufo_buffer_get_size(buffer);

    cl_mem_flags mem_flags = CL_MEM_READ_WRITE;
    if ((data != NULL) && (command_queue != NULL))
        mem_flags |= CL_MEM_COPY_HOST_PTR;
    
    cl_int errcode;
    cl_mem buffer_mem = clCreateBuffer(priv->opencl_context,
            mem_flags, 
            num_bytes,
            data, &errcode);
    CHECK_ERROR(errcode);

    ufo_buffer_set_cl_mem(buffer, buffer_mem);

    if ((data) && (command_queue == NULL))
        ufo_buffer_set_host_array(buffer, data, num_bytes, NULL);

    return buffer;
}

/**
 * ufo_resource_manager_memalloc:
 * @manager: A #UfoResourceManager
 * @size: Size of cl_mem in bytes
 *
 * Allocates a new cl_mem object with the given size.
 *
 * Return value: A cl_mem object
 */
gpointer ufo_resource_manager_memalloc(UfoResourceManager *manager, gsize size)
{
    cl_int errcode = CL_SUCCESS;
    cl_mem mem = clCreateBuffer(manager->priv->opencl_context, CL_MEM_READ_WRITE, size, NULL, &errcode);
    CHECK_ERROR(errcode);
    return mem;
}

/**
 * ufo_resource_manager_memdup:
 * @manager: A #UfoResourceManager
 * @memobj: A cl_mem object
 *
 * Creates a new cl_mem object with the same size as a given cl_mem object.
 *
 * Return value: A new cl_mem object
 */
gpointer ufo_resource_manager_memdup(UfoResourceManager *manager, gpointer memobj)
{
    size_t size = 0;
    cl_mem mem = (cl_mem) memobj;
    CHECK_ERROR(clGetMemObjectInfo(mem, CL_MEM_SIZE, sizeof(size_t), &size, NULL));
    cl_mem dup = ufo_resource_manager_memalloc(manager, size);
    return dup;
}

/**
 * ufo_resource_manager_release_buffer:
 * @manager: A #UfoResourceManager
 * @buffer: A #UfoBuffer
 *
 * Release the memory of this buffer.
 */
void ufo_resource_manager_release_buffer(UfoResourceManager *manager, UfoBuffer *buffer)
{
#ifdef WITH_PROFILING
    gulong upload_time, download_time;
    ufo_buffer_get_transfer_time(buffer, &upload_time, &download_time);
    priv->upload_time += upload_time / 1000000000.0;
    priv->download_time += download_time / 1000000000.0;
#endif

    cl_mem mem = (cl_mem) ufo_buffer_get_cl_mem(buffer);
    if (mem != NULL)
        CHECK_ERROR(clReleaseMemObject(mem));

    g_object_unref(buffer);
}

guint ufo_resource_manager_get_new_id(UfoResourceManager *resource_manager)
{
    guint id;
    static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
    g_static_mutex_lock(&mutex);
    id = resource_manager->priv->current_id++;
    g_static_mutex_unlock(&mutex);
    return id;
}

/**
 * @ufo_resource_manager_get_command_queues:
 * @manager: A #UfoResourceManager
 * @command_queues: (out) (transfer: none): Sets pointer to command_queues array
 * @num_queues: (out): Number of queues
 *
 * Return the number and actual command queues.
 */
void ufo_resource_manager_get_command_queues(UfoResourceManager *manager, gpointer *command_queues, int *num_queues)
{
    /* FIXME: Use only first platform */
    *num_queues = manager->priv->num_devices[0];
    *command_queues = manager->priv->command_queues;
}

/**
 * @ufo_resource_manager_get_number_of_devices:
 * @manager: A #UfoResourceManager
 *
 * Return value: Number of acceleration devices such as GPUs used by the
 * resource manager.
 */
guint ufo_resource_manager_get_number_of_devices(UfoResourceManager *manager)
{
    return manager->priv->num_devices[0];
}

static void ufo_resource_manager_finalize(GObject *gobject)
{
    UfoResourceManager *self = UFO_RESOURCE_MANAGER(gobject);
    UfoResourceManagerPrivate *priv = UFO_RESOURCE_MANAGER_GET_PRIVATE(self);

#ifdef WITH_PROFILING
    g_message("Memory transfer time between host and device");
    g_message("  To Device: %.4lfs", priv->upload_time);
    g_message("  To Host..: %.4lfs", priv->download_time);
    g_message("  Total....: %.4lfs", priv->upload_time + priv->download_time);
#endif

    g_hash_table_destroy(priv->opencl_kernels);

    g_list_foreach(priv->opencl_files, (GFunc) g_free, NULL);
    g_list_free(priv->opencl_files);

    g_list_foreach(priv->opencl_kernel_table, (GFunc) g_free, NULL);
    g_list_free(priv->opencl_kernel_table);

    g_list_foreach(priv->opencl_programs, resource_manager_release_program, NULL);
    g_list_free(priv->opencl_programs);

    for (int i = 0; i < priv->num_devices[0]; i++)
        clReleaseCommandQueue(priv->command_queues[i]);

    CHECK_ERROR(clReleaseContext(priv->opencl_context));

    g_string_free(priv->opencl_build_options, TRUE);

    for (guint i = 0; i < priv->num_platforms; i ++)
        g_free(priv->opencl_devices[i]);

    g_free(priv->num_devices);
    g_free(priv->opencl_devices);
    g_free(priv->opencl_platforms);
    g_free(priv->command_queues);

    priv->num_devices = NULL;
    priv->opencl_kernels = NULL;
    priv->opencl_devices = NULL;
    priv->opencl_platforms = NULL;

    G_OBJECT_CLASS(ufo_resource_manager_parent_class)->finalize(gobject);
}

static void ufo_resource_manager_class_init(UfoResourceManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = ufo_resource_manager_finalize;

    g_type_class_add_private(klass, sizeof(UfoResourceManagerPrivate));
}

static void ufo_resource_manager_init(UfoResourceManager *self)
{
    UfoResourceManagerPrivate *priv;

    self->priv = priv = UFO_RESOURCE_MANAGER_GET_PRIVATE(self);

    priv->upload_time = 0.0f;
    priv->download_time = 0.0f;

    priv->paths = g_strdup_printf(".");
    priv->opencl_kernel_table = NULL;
    priv->opencl_kernels = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, resource_manager_release_kernel);
    priv->opencl_platforms = NULL;
    priv->opencl_programs = NULL;
    priv->opencl_build_options = g_string_new("-cl-mad-enable ");

    priv->current_id = 0;

    /* initialize OpenCL subsystem */
    int errcode = CL_SUCCESS;
    CHECK_ERROR(clGetPlatformIDs(0, NULL, &priv->num_platforms));
    priv->opencl_platforms = g_malloc0(priv->num_platforms * sizeof(cl_platform_id));
    CHECK_ERROR(clGetPlatformIDs(priv->num_platforms, priv->opencl_platforms, NULL));
    priv->num_devices = g_malloc0(priv->num_platforms * sizeof(cl_uint));
    priv->opencl_devices = g_malloc0(priv->num_platforms * sizeof(cl_device_id *));
    g_debug("Number of OpenCL platforms: %i", priv->num_platforms);

    /* Get devices for each available platform */
    gchar *info_buffer = g_malloc0(256);
    for (int i = 0; i < priv->num_platforms; i++) {
        cl_platform_id platform = priv->opencl_platforms[i];

        CHECK_ERROR(clGetPlatformInfo(platform, CL_PLATFORM_NAME, 256, info_buffer, NULL));
        g_debug("--- %s ---", info_buffer);

        CHECK_ERROR(clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, 256, info_buffer, NULL));
        g_debug(" Vendor...........: %s", info_buffer);

        if (g_str_has_prefix(info_buffer, "NVIDIA"))
            g_string_append(priv->opencl_build_options, "-cl-nv-verbose -DVENDOR=NVIDIA");
        else if (g_str_has_prefix(info_buffer, "Advanced Micro Devices"))
            g_string_append(priv->opencl_build_options, "-DVENDOR=AMD");

        CHECK_ERROR(clGetPlatformInfo(platform, CL_PLATFORM_VERSION, 256, info_buffer, NULL));
        g_debug(" Version..........: %s", info_buffer);

        cl_uint num_devices;
        CHECK_ERROR(clGetDeviceIDs(platform,
                CL_DEVICE_TYPE_ALL,
                0, NULL,
                &num_devices));

        priv->opencl_devices[i] = g_malloc0(num_devices * sizeof(cl_device_id));

        CHECK_ERROR(clGetDeviceIDs(platform,
                CL_DEVICE_TYPE_ALL,
                num_devices, priv->opencl_devices[i],
                NULL));

        priv->num_devices[i] = num_devices;
        g_debug(" Number of devices: %i", num_devices);
        g_debug(" Build options....: %s", priv->opencl_build_options->str);
    }
    g_free(info_buffer);

    cl_command_queue_properties queue_properties = 0;
#ifdef WITH_PROFILING
    queue_properties = CL_QUEUE_PROFILING_ENABLE;
#endif

    /* XXX: create context for each platform?! */
    if (priv->num_platforms > 0) {
        priv->opencl_context = clCreateContext(NULL, 
                priv->num_devices[0],
                priv->opencl_devices[0],
                NULL, NULL, &errcode);

        CHECK_ERROR(errcode);

        priv->command_queues = g_malloc0(priv->num_devices[0] * sizeof(cl_command_queue));

        for (int i = 0; i < priv->num_devices[0]; i++) {
            priv->command_queues[i] = clCreateCommandQueue(priv->opencl_context,
                    priv->opencl_devices[0][i],
                    queue_properties, &errcode);
            CHECK_ERROR(errcode);
        }
    }
}
