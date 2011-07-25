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
    /* OpenCL related */
    cl_uint num_platforms;
    cl_platform_id *opencl_platforms;

    cl_uint *num_devices;               /**< Number of OpenCL devices per platform id */
    cl_device_id **opencl_devices;      /**< Array of OpenCL devices per platform id */
    cl_context opencl_context;

    cl_command_queue *command_queues;   /**< Array of command queues per device */

    GList *opencl_files;
    GList *opencl_kernel_table;
    GList *opencl_programs;
    GString *opencl_build_options;

    GHashTable *opencl_kernels;         /**< maps from kernel string to cl_kernel */

    float upload_time;
    float download_time;
};

static const gchar* opencl_error_msgs[] = {
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
const gchar* opencl_map_error(cl_int error)
{
    if (error >= -14)
        return opencl_error_msgs[-error];
    if (error <= -30)
        return opencl_error_msgs[-error-15];
    return NULL;
}

#define CHECK_ERROR(error) { \
    if (error != CL_SUCCESS) g_message("OpenCL error <%s:%i>: %s", __FILE__, __LINE__, opencl_map_error((error))); }

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

static void resource_manager_release_kernel(gpointer data, gpointer user_data)
{
    cl_kernel kernel = (cl_kernel) data;
    clReleaseKernel(kernel);
}

static void resource_manager_release_program(gpointer data, gpointer user_data)
{
    cl_program program = (cl_program) data;
    clReleaseProgram(program);
}

/*static UfoBuffer *resource_manager_create_buffer(UfoResourceManager* self,*/
        /*guint32 width, */
        /*guint32 height,*/
        /*float *data)*/
/*{*/
    /*return buffer;*/
/*}*/

GQuark ufo_resource_manager_error_quark(void)
{
    return g_quark_from_static_string("ufo-resource-manager-error-quark");
}

/* 
 * Public Interface
 */
/**
 * \brief Return a UfoResourceManager instance
 * \public \memberof UfoResourceManager
 * \return A UfoResourceManager
 */
UfoResourceManager *ufo_resource_manager()
{
    static UfoResourceManager *manager = NULL;
    if (manager == NULL)
        manager = UFO_RESOURCE_MANAGER(g_object_new(UFO_TYPE_RESOURCE_MANAGER, NULL));

    return manager; 
}

/**
 * \brief Adds an OpenCL program and loads all kernels in that file
 * \param[in] resource_manager A UfoResourceManager
 * \param[in] filename File containing valid OpenCL code
 * \param[in] options Additional build options such as "-DX=Y", can be NULL
 * \param[out] error Pointer to a GError* 
 * \public \memberof UfoResourceManager
 * \return TRUE on success else FALSE
 */
gboolean ufo_resource_manager_add_program(
        UfoResourceManager *resource_manager, 
        const gchar *filename, 
        const gchar *options,
        GError **error)
{
    UfoResourceManagerPrivate *priv = resource_manager->priv;

    /* Don't process the kernel file again, if already load */
    if (g_list_find_custom(priv->opencl_files, filename, (GCompareFunc) g_strcmp0))
        return TRUE;

    gchar *buffer = resource_manager_load_opencl_program(filename);
    if (buffer == NULL) {
        g_set_error(error,
                UFO_RESOURCE_MANAGER_ERROR,
                UFO_RESOURCE_MANAGER_ERROR_LOAD_PROGRAM,
                "Failed to open file: %s",
                filename);
        return FALSE;
    }
    priv->opencl_files = g_list_append(priv->opencl_files, g_strdup(filename));

    int err = CL_SUCCESS;
    cl_program program = clCreateProgramWithSource(priv->opencl_context,
            1, (const char **) &buffer, NULL, &err);

    if (err != CL_SUCCESS) {
        g_set_error(error,
                UFO_RESOURCE_MANAGER_ERROR,
                UFO_RESOURCE_MANAGER_ERROR_CREATE_PROGRAM,
                "Failed to create OpenCL program: %s", opencl_map_error(err));
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
    err = clBuildProgram(program, 
            priv->num_devices[0], 
            priv->opencl_devices[0],
            build_options,
            NULL, NULL);

    g_message("Build options: %s", build_options);

    if (options)
        g_free(build_options);

    const int LOG_SIZE = 4096;
    gchar* log = (gchar *) g_malloc0(LOG_SIZE * sizeof(char));
    clGetProgramBuildInfo(program, priv->opencl_devices[0][0], 
            CL_PROGRAM_BUILD_LOG, LOG_SIZE, (void*) log, NULL);
    g_print("\n=== Build log for %s===%s\n\n", filename, log);

    if (err != CL_SUCCESS) {
        g_set_error(error,
                UFO_RESOURCE_MANAGER_ERROR,
                UFO_RESOURCE_MANAGER_ERROR_BUILD_PROGRAM,
                "Failed to build OpenCL program");
        g_print("\n=== Build log for %s===%s\n\n", filename, log);
        g_free(log);
        g_free(buffer);
        return FALSE;
    }
    g_free(log);

    /* Create all kernels in the program source and map their function names to
     * the corresponding cl_kernel object */
    cl_uint num_kernels;
    clCreateKernelsInProgram(program, 0, NULL, &num_kernels);
    cl_kernel *kernels = (cl_kernel *) g_malloc0(num_kernels * sizeof(cl_kernel));
    clCreateKernelsInProgram(program, num_kernels, kernels, NULL);
    priv->opencl_kernel_table = g_list_append(priv->opencl_kernel_table, kernels);

    for (guint i = 0; i < num_kernels; i++) {
        size_t kernel_name_length;    
        clGetKernelInfo(kernels[i], CL_KERNEL_FUNCTION_NAME, 
                0, NULL, 
                &kernel_name_length);

        gchar *kernel_name = (gchar *) g_malloc0(kernel_name_length);
        clGetKernelInfo(kernels[i], CL_KERNEL_FUNCTION_NAME, 
                        kernel_name_length, kernel_name, 
                        NULL);

        /*g_debug("Add OpenCL kernel '%s'", kernel_name);*/
        g_hash_table_insert(priv->opencl_kernels, kernel_name, kernels[i]);
    }

    g_free(buffer);
    return TRUE;
}

/**
 * \brief Retrieve a loaded OpenCL kernel
 * \public \memberof UfoResourceManager
 * \param[in] resource_manager A UfoResourceManager
 * \param[in] kernel_name Zero-delimited string of the kernel name
 * \param[out] error Pointer to a GError* 
 * \note The kernel must be contained in one of the files that has been
 *      previously loaded with ufo_resource_manager_add_program()
 * \return A cl_kernel object
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
    clRetainKernel(kernel);
    return kernel;
}

/**
 * \brief Return an OpenCL context object
 *
 * This method can be used to initialize third-party libraries like Apple's FFT
 * library.
 *
 * \param[in] resource_manager A UfoResourceManager
 * \return A cl_context object
 */
gpointer ufo_resource_manager_get_context(UfoResourceManager *resource_manager)
{
    return resource_manager->priv->opencl_context; 
}

/**
 * \brief Request a UfoBuffer with a given size
 * \public \memberof UfoResourceManager
 * \param[in] resource_manager A UfoResourceManager
 * \param[in] width Width of the buffer
 * \param[in] height Height of the buffer
 * \param[in] data Initial floating point data array with at least width*height
 *      size. If data is NULL, nothing is copied onto a device. If non-floating
 *      point types have to be uploaded, use ufo_buffer_set_cpu_data() and
 *      ufo_buffer_reinterpret() on the return UfoBuffer.
 *
 * \return A UfoBuffer object
 *
 * \note If you don't intend to use the buffer or won't push it again to the
 *      next UfoElement, it has to be released with
 *      ufo_resource_manager_release_buffer()
 */
UfoBuffer *ufo_resource_manager_request_buffer(UfoResourceManager *resource_manager, 
        guint32 width, 
        guint32 height,
        float *data,
        gboolean prefer_gpu)
{
    UfoResourceManager *self = resource_manager;
    UfoBuffer *buffer = ufo_buffer_new(width, height);

    cl_mem_flags mem_flags = CL_MEM_READ_WRITE;
    if ((data != NULL) && (prefer_gpu))
        mem_flags |= CL_MEM_COPY_HOST_PTR;

    cl_mem buffer_mem = clCreateBuffer(self->priv->opencl_context,
            mem_flags, 
            width * height * sizeof(float),
            data, NULL);

    ufo_buffer_set_cl_mem(buffer, buffer_mem);
    if ((data) && (!prefer_gpu))
        ufo_buffer_set_cpu_data(buffer, data, width*height*sizeof(float), NULL);

    return buffer;
}

UfoBuffer *ufo_resource_manager_request_finish_buffer(UfoResourceManager *self)
{
    UfoBuffer *buffer = ufo_buffer_new(1, 1);
    /* TODO: make "finished" constructable? How to do ufo_buffer_new? */
    g_object_set(buffer, "finished", TRUE, NULL);
    return buffer;
}

UfoBuffer *ufo_resource_manager_copy_buffer(UfoResourceManager *manager, UfoBuffer *buffer)
{
    gint32 width, height;
    ufo_buffer_get_dimensions(buffer, &width, &height);
    /* XXX: Use first command queue, which might not be the best in all cases */
    float *data = ufo_buffer_get_cpu_data(buffer, manager->priv->command_queues);
    UfoBuffer *copy = ufo_resource_manager_request_buffer(manager,
            width, height, data, FALSE);
    return copy;
}

/**
 * \brief Release a UfoBuffer for further use
 * \public \memberof UfoResourceManager
 *
 * \param[in] resource_manager A UfoResourceManager
 * \param[in] buffer A UfoBuffer
 */
void ufo_resource_manager_release_buffer(UfoResourceManager *resource_manager, UfoBuffer *buffer)
{
    UfoResourceManager *self = resource_manager;

#ifdef WITH_PROFILING
    gulong upload_time, download_time;
    ufo_buffer_get_transfer_time(buffer, &upload_time, &download_time);
    self->priv->upload_time += upload_time / 1000000000.0;
    self->priv->download_time += download_time / 1000000000.0;
#endif

    cl_command_queue command_queue = self->priv->command_queues[0];
    cl_mem mem = ufo_buffer_get_gpu_data(buffer, command_queue);
    if (mem != NULL)
        clReleaseMemObject(mem);

    g_object_unref(buffer);
}

void ufo_resource_manager_get_command_queues(UfoResourceManager *resource_manager, gpointer *command_queues, guint *num_queues)
{
    /* FIXME: Use only first platform */
    *num_queues = resource_manager->priv->num_devices[0];
    *command_queues = resource_manager->priv->command_queues;
}


/* 
 * Virtual Methods
 */
static void ufo_resource_manager_dispose(GObject *gobject)
{
    UfoResourceManager *self = UFO_RESOURCE_MANAGER(gobject);
    UfoResourceManagerPrivate *priv = UFO_RESOURCE_MANAGER_GET_PRIVATE(self);

    g_message("Upload/Download [s]: %f/%f", priv->upload_time, priv->download_time);
    g_message("Total Transfer Time [s]: %f", priv->upload_time + priv->download_time);

    /* free resources */
    GList *kernels = g_hash_table_get_values(priv->opencl_kernels);
    g_list_foreach(kernels, resource_manager_release_kernel, NULL);
    g_list_free(kernels);

    GList *kernel_names = g_hash_table_get_keys(priv->opencl_kernels);
    g_list_foreach(kernel_names, (GFunc) g_free, NULL);
    g_list_free(kernel_names);
    g_hash_table_destroy(priv->opencl_kernels);

    g_list_foreach(priv->opencl_kernel_table, (GFunc) g_free, NULL);
    g_list_free(priv->opencl_kernel_table);

    g_list_foreach(priv->opencl_programs, resource_manager_release_program, NULL);
    g_list_free(priv->opencl_programs);

    for (int i = 0; i < priv->num_devices[0]; i++)
        clReleaseCommandQueue(priv->command_queues[i]);
    g_free(priv->command_queues);
    clReleaseContext(priv->opencl_context);

    g_list_foreach(priv->opencl_files, (GFunc) g_free, NULL);
    g_list_free(priv->opencl_files);

    g_string_free(priv->opencl_build_options, TRUE);

    G_OBJECT_CLASS(ufo_resource_manager_parent_class)->dispose(gobject);
}

static void ufo_resource_manager_finalize(GObject *gobject)
{
    UfoResourceManager *self = UFO_RESOURCE_MANAGER(gobject);
    UfoResourceManagerPrivate *priv = UFO_RESOURCE_MANAGER_GET_PRIVATE(self);

    for (guint i = 0; i < priv->num_platforms; i ++)
        g_free(priv->opencl_devices[i]);
    g_free(priv->num_devices);
    g_free(priv->opencl_devices);
    g_free(priv->opencl_platforms);
    priv->num_devices = NULL;
    priv->opencl_devices = NULL;
    priv->opencl_platforms = NULL;

    G_OBJECT_CLASS(ufo_resource_manager_parent_class)->finalize(gobject);
}

/*
 * Type/Class Initialization
 */
static void ufo_resource_manager_class_init(UfoResourceManagerClass *klass)
{
    /* override GObject methods */
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = ufo_resource_manager_dispose;
    gobject_class->finalize = ufo_resource_manager_finalize;

    /* install private data */
    g_type_class_add_private(klass, sizeof(UfoResourceManagerPrivate));
}

static void ufo_resource_manager_init(UfoResourceManager *self)
{
    UfoResourceManagerPrivate *priv;
    cl_int error;

    self->priv = priv = UFO_RESOURCE_MANAGER_GET_PRIVATE(self);

    priv->upload_time = 0.0f;
    priv->download_time = 0.0f;
    priv->opencl_kernel_table = NULL;
    priv->opencl_kernels = g_hash_table_new(g_str_hash, g_str_equal);
    priv->opencl_platforms = NULL;
    priv->opencl_programs = NULL;
    priv->opencl_build_options = g_string_new("-cl-mad-enable ");

    /* initialize OpenCL subsystem */
    clGetPlatformIDs(0, NULL, &priv->num_platforms);
    priv->opencl_platforms = g_malloc0(priv->num_platforms * sizeof(cl_platform_id));
    clGetPlatformIDs(priv->num_platforms, priv->opencl_platforms, NULL);
    priv->num_devices = g_malloc0(priv->num_platforms * sizeof(cl_uint));
    priv->opencl_devices = g_malloc0(priv->num_platforms * sizeof(cl_device_id *));
    g_debug("Number of OpenCL platforms: %i", priv->num_platforms);

    /* Get devices for each available platform */
    /* TODO: maybe merge all devices into one big list? */
    gchar *info_buffer = g_malloc0(256);
    for (int i = 0; i < priv->num_platforms; i ++) {
        cl_platform_id platform = priv->opencl_platforms[i];

        clGetPlatformInfo(platform, CL_PLATFORM_NAME, 256, info_buffer, NULL);
        g_debug("--- %s ---", info_buffer);

        clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, 256, info_buffer, NULL);
        g_debug(" Vendor...........: %s", info_buffer);
        if (g_str_has_prefix(info_buffer, "NVIDIA"))
            g_string_append(priv->opencl_build_options, "-cl-nv-verbose ");

        clGetPlatformInfo(platform, CL_PLATFORM_VERSION, 256, info_buffer, NULL);
        g_debug(" Version..........: %s", info_buffer);

        cl_uint num_devices;

        clGetDeviceIDs(platform,
                CL_DEVICE_TYPE_ALL,
                0, NULL,
                &num_devices);

        priv->opencl_devices[i] = g_malloc0(num_devices * sizeof(cl_device_id));

        error = clGetDeviceIDs(platform,
                CL_DEVICE_TYPE_ALL,
                num_devices, priv->opencl_devices[i],
                NULL);

        CHECK_ERROR(error);

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
                NULL, NULL, &error);

        CHECK_ERROR(error);

        priv->command_queues = g_malloc0(priv->num_devices[0] * sizeof(cl_command_queue));
        for (int i = 0; i < priv->num_devices[0]; i++) {
            priv->command_queues[i] = clCreateCommandQueue(priv->opencl_context,
                    priv->opencl_devices[0][i],
                    queue_properties, &error);

            CHECK_ERROR(error);
        }
    }
}
