#include <glib.h>
#include <stdio.h>

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

    cl_uint *num_devices;
    cl_device_id **opencl_devices;
    cl_context opencl_context;

    /* FIXME: replace with multiple queues */
    cl_command_queue command_queue;

    GList *opencl_kernel_table;
    GList *opencl_programs;

    GList *buffers;                 /**< keep a list for buffer destruction */
    GHashTable *buffer_map;         /**< maps from dimension hash to a queue of buffer instances */
    GHashTable *opencl_kernels;     /**< maps from kernel string to cl_kernel */
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


static guint32 resource_manager_hash_dims(guint32 width, guint32 height)
{
    guint32 result = 0x345678;
    result ^= width << 12;
    result ^= height;
    return result;
}

static gchar *resource_manager_load_opencl_program(const gchar *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
        return NULL;

    fseek(fp, 0, SEEK_END);
    const size_t length = ftell(fp);
    rewind(fp);

    char *buffer = (char *) g_malloc0(length);
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

static void *resource_manager_release_kernel(gpointer data, gpointer user_data)
{
    cl_kernel kernel = (cl_kernel) data;
    clReleaseKernel(kernel);
    return NULL;
}

static void *resource_manager_release_program(gpointer data, gpointer user_data)
{
    cl_program program = (cl_program) data;
    clReleaseProgram(program);
    return NULL;
}

static void *resource_manager_release_mem(gpointer data, gpointer user_data)
{
    UfoBuffer *buffer = UFO_BUFFER(data);
    cl_mem mem = ufo_buffer_get_gpu_data(buffer);
    if (mem != NULL)
        clReleaseMemObject(mem);
    g_object_unref(buffer);
    return NULL;
}

static UfoBuffer *resource_manager_create_buffer(UfoResourceManager* self,
        guint32 width, 
        guint32 height,
        float *data)
{
    UfoBuffer *buffer = ufo_buffer_new(width, height);
    self->priv->buffers = g_list_append(self->priv->buffers, buffer);

    /* TODO 1: Let user specify access flags */
    /* TODO 2: Think about copy strategy */
    cl_mem_flags mem_flags = CL_MEM_READ_WRITE;
    if (data != NULL) {
        /*mem_flags |= CL_MEM_COPY_HOST_PTR;*/
        ufo_buffer_set_cpu_data(buffer, data, width*height*sizeof(float), NULL);
    }

    cl_mem buffer_mem = clCreateBuffer(self->priv->opencl_context,
            mem_flags, 
            width * height * sizeof(float),
            NULL, NULL);

    ufo_buffer_set_cl_mem(buffer, buffer_mem);
    ufo_buffer_set_command_queue(buffer, self->priv->command_queue);
    return buffer;
}

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
 * \param[out] error Pointer to a GError* 
 * \public \memberof UfoResourceManager
 * \return TRUE on success else FALSE
 */
gboolean ufo_resource_manager_add_program(UfoResourceManager *resource_manager, const gchar *filename, GError **error)
{
    UfoResourceManagerPrivate *priv = resource_manager->priv;

    gchar *buffer = resource_manager_load_opencl_program(filename);
    if (buffer == NULL) {
        g_set_error(error,
                UFO_RESOURCE_MANAGER_ERROR,
                UFO_RESOURCE_MANAGER_ERROR_LOAD_PROGRAM,
                "Failed to open file: %s",
                filename);
        return FALSE;
    }

    int err = CL_SUCCESS;
    cl_program program = clCreateProgramWithSource(priv->opencl_context,
            1, (const char **) &buffer, NULL, &err);
    if (err != CL_SUCCESS) {
        g_set_error(error,
                UFO_RESOURCE_MANAGER_ERROR,
                UFO_RESOURCE_MANAGER_ERROR_CREATE_PROGRAM,
                "Failed to create OpenCL program");
        g_free(buffer);
        return FALSE;
    }
    priv->opencl_programs = g_list_append(priv->opencl_programs, program);

    /* TODO: build program for each platform?!*/
    err = clBuildProgram(program, 
            priv->num_devices[0], 
            priv->opencl_devices[0],
            NULL, NULL, NULL);

    if (err != CL_SUCCESS) {
        g_set_error(error,
                UFO_RESOURCE_MANAGER_ERROR,
                UFO_RESOURCE_MANAGER_ERROR_BUILD_PROGRAM,
                "Failed to build OpenCL program");
        g_free(buffer);
        return FALSE;
    }

    /* Create all kernels in the program source and map their function names to
     * the corresponding cl_kernel object */
    cl_uint num_kernels;
    clCreateKernelsInProgram(program, 0, NULL, &num_kernels);
    /* FIXME: we are leaking kernels because we don't keep the kernels base
     * address */
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
        g_debug("Add OpenCL kernel '%s'", kernel_name);
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
        float *data)
{
    UfoResourceManager *self = resource_manager;
    const gpointer hash = GINT_TO_POINTER(resource_manager_hash_dims(width, height));

    GQueue *queue = g_hash_table_lookup(self->priv->buffer_map, hash);
    UfoBuffer *buffer = NULL;

    if (queue == NULL) {
        /* If there is no queue for this hash we create a new one but don't fill
         * it with the newly created buffer */
        buffer = resource_manager_create_buffer(self, width, height, data);
        queue = g_queue_new();
        g_hash_table_insert(self->priv->buffer_map, hash, queue);
    }
    else {
        buffer = g_queue_pop_head(queue);
        if (buffer == NULL)
            buffer = resource_manager_create_buffer(self, width, height, data);
        else if (data != NULL)
            ufo_buffer_set_cpu_data(buffer, data, width*height*sizeof(float), NULL);
    }
    
    return buffer;
}

UfoBuffer *ufo_resource_manager_request_finish_buffer(UfoResourceManager *self)
{
    UfoBuffer *buffer = ufo_buffer_new(1, 1);
    self->priv->buffers = g_list_append(self->priv->buffers, buffer);
    /* TODO: make "finished" constructable? How to do ufo_buffer_new? */
    g_object_set(buffer, "finished", TRUE, NULL);
    return buffer;
}

UfoBuffer *ufo_resource_manager_copy_buffer(UfoResourceManager *manager, UfoBuffer *buffer)
{
    gint32 width, height;
    ufo_buffer_get_dimensions(buffer, &width, &height);
    float *data = ufo_buffer_get_cpu_data(buffer);
    UfoBuffer *copy = ufo_resource_manager_request_buffer(manager,
            width, height, data);
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
    gint32 width, height;
    ufo_buffer_get_dimensions(buffer, &width, &height);
    const gpointer hash = GINT_TO_POINTER(resource_manager_hash_dims(width, height));

    GQueue *queue = g_hash_table_lookup(self->priv->buffer_map, hash);
    if (queue == NULL) { /* should not be the case */
        queue = g_queue_new();
        g_hash_table_insert(self->priv->buffer_map, hash, queue);
    }
    g_queue_push_head(queue, buffer); 
}


/* 
 * Virtual Methods
 */
static void ufo_resource_manager_dispose(GObject *gobject)
{
    UfoResourceManager *self = UFO_RESOURCE_MANAGER(gobject);
    UfoResourceManagerPrivate *priv = UFO_RESOURCE_MANAGER_GET_PRIVATE(self);

    GList *kernels = g_hash_table_get_values(priv->opencl_kernels);
    GList *kernel_names = g_hash_table_get_keys(priv->opencl_kernels);
    g_list_foreach(kernel_names, (GFunc) g_free, NULL);
    g_list_foreach(kernels, (gpointer) resource_manager_release_kernel, NULL);
    g_list_free(kernels);
    g_list_free(kernel_names);

    g_list_foreach(priv->opencl_kernel_table, (GFunc) g_free, NULL);
    g_list_foreach(priv->opencl_programs, (gpointer) resource_manager_release_program, NULL);
    g_list_foreach(priv->buffers, (gpointer) resource_manager_release_mem, NULL);
    clReleaseCommandQueue(priv->command_queue);
    clReleaseContext(priv->opencl_context);

    GList *buffers = g_hash_table_get_values(priv->buffer_map);
    g_list_foreach(buffers, (GFunc) g_queue_free, NULL);
    g_list_free(buffers);

    g_hash_table_destroy(priv->buffer_map);
    g_hash_table_destroy(priv->opencl_kernels);
    g_list_free(priv->opencl_kernel_table);
    g_list_free(priv->opencl_programs);
    g_list_free(priv->buffers);

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

    priv->buffers = NULL;
    priv->buffer_map = g_hash_table_new(NULL, NULL);
    priv->opencl_kernel_table = NULL;
    priv->opencl_kernels = g_hash_table_new(g_str_hash, g_str_equal);
    priv->opencl_platforms = NULL;
    priv->opencl_programs = NULL;

    /* initialize OpenCL subsystem */
    clGetPlatformIDs(0, NULL, &priv->num_platforms);
    priv->opencl_platforms = g_malloc0(priv->num_platforms * sizeof(cl_platform_id));
    clGetPlatformIDs(priv->num_platforms, priv->opencl_platforms, NULL);
    priv->num_devices = g_malloc0(priv->num_platforms * sizeof(cl_uint));
    priv->opencl_devices = g_malloc0(priv->num_platforms * sizeof(cl_device_id *));
    g_debug("Number of OpenCL platforms: %i", priv->num_platforms);

    /* Get devices for each available platform */
    /* TODO: maybe merge all devices into one big list? */
    for (guint i = 0; i < priv->num_platforms; i ++) {
        cl_platform_id platform = priv->opencl_platforms[i];
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

        priv->num_devices[i] = num_devices;
        g_debug("Number of OpenCL devices on platform %i: %i", i, num_devices);
    }

    /* XXX: create context for each platform?! */
    if (priv->num_platforms > 0) {
        priv->opencl_context = clCreateContext(NULL, 
                priv->num_devices[0],
                priv->opencl_devices[0],
                NULL, NULL, &error);

        priv->command_queue = clCreateCommandQueue(priv->opencl_context,
                priv->opencl_devices[0][0],
                0, NULL);
    }
}
