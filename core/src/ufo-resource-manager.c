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

    GList *opencl_programs;

    GHashTable *buffers;            /**< maps from dimension hash to a GTrashStack of buffer instances */
    GHashTable *opencl_kernels;     /**< maps from kernel string to cl_kernel */
};

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

GQuark ufo_resource_manager_error_quark(void)
{
    return g_quark_from_static_string("ufo-resource-manager-error-quark");
}

/* 
 * Public Interface
 */
/**
 * \brief Create a new UfoResourceManager instance
 * \public \memberof UfoResourceManager
 * \return A UfoResourceManager
 */
UfoResourceManager *ufo_resource_manager_new()
{
    return UFO_RESOURCE_MANAGER(g_object_new(UFO_TYPE_RESOURCE_MANAGER, NULL));
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
    UfoResourceManager *self = resource_manager;
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
    cl_program program = clCreateProgramWithSource(self->priv->opencl_context,
            1, (const char **) &buffer, NULL, &err);
    if (err != CL_SUCCESS) {
        g_set_error(error,
                UFO_RESOURCE_MANAGER_ERROR,
                UFO_RESOURCE_MANAGER_ERROR_CREATE_PROGRAM,
                "Failed to create OpenCL program");
        return FALSE;
    }
    self->priv->opencl_programs = g_list_append(self->priv->opencl_programs, program);

    /* TODO: build program for each platform?!*/
    err = clBuildProgram(program, 
            self->priv->num_devices[0], 
            self->priv->opencl_devices[0],
            NULL, NULL, NULL);

    if (err != CL_SUCCESS) {
        g_set_error(error,
                UFO_RESOURCE_MANAGER_ERROR,
                UFO_RESOURCE_MANAGER_ERROR_BUILD_PROGRAM,
                "Failed to build OpenCL program");
        return FALSE;
    }

    /* Create all kernels in the program source and map their function names to
     * the corresponding cl_kernel object */
    cl_uint num_kernels;
    clCreateKernelsInProgram(program, 0, NULL, &num_kernels);
    cl_kernel *kernels = (cl_kernel *) g_malloc0(num_kernels * sizeof(cl_kernel));
    clCreateKernelsInProgram(program, num_kernels, kernels, NULL);

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
        g_hash_table_insert(self->priv->opencl_kernels, kernel_name, kernels[i]);
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
cl_kernel ufo_resource_manager_get_kernel(UfoResourceManager *resource_manager, const gchar *kernel_name, GError **error)
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
    return kernel;
}

/**
 * \brief Request a UfoBuffer with a given size
 * \public \memberof UfoResourceManager
 * \param[in] resource_manager A UfoResourceManager
 * \param[in] width Width of the buffer
 * \param[in] height Height of the buffer
 * \note If you don't intend to use the buffer or won't push it again to the
 *      next UfoElement, it has to be released with
 *      ufo_resource_manager_release_buffer()
 * \return A UfoBuffer object
 */
UfoBuffer *ufo_resource_manager_request_buffer(UfoResourceManager *resource_manager, guint32 width, guint32 height)
{
    UfoResourceManager *self = resource_manager;
    const gpointer hash = GINT_TO_POINTER(resource_manager_hash_dims(width, height));

    GQueue *queue = g_hash_table_lookup(self->priv->buffers, hash);
    UfoBuffer *buffer = NULL;

    if (queue == NULL) {
        /* If there is no queue for this hash we create a new one but don't fill
         * it with the newly created buffer */
        buffer = ufo_buffer_new(width, height);
        /* TODO 1: Let user specify access flags */
        /* TODO 2: Let user pass initial buffer */
        /* TODO 3: Release buffer_mem */
        cl_mem buffer_mem = clCreateBuffer(self->priv->opencl_context,
                CL_MEM_READ_WRITE, 
                width * height * sizeof(float),
                NULL, NULL);

        ufo_buffer_set_cl_mem(buffer, buffer_mem);
        ufo_buffer_set_command_queue(buffer, self->priv->command_queue);

        queue = g_queue_new();
        g_hash_table_insert(self->priv->buffers, hash, queue);
    }
    else {
        buffer = g_queue_pop_head(queue);
        if (buffer == NULL)
            buffer = ufo_buffer_new(width, height);
    }
    
    return buffer;
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

    GQueue *queue = g_hash_table_lookup(self->priv->buffers, hash);
    if (queue == NULL) { /* should not be the case */
        queue = g_queue_new();
        g_hash_table_insert(self->priv->buffers, hash, queue);
    }
    g_queue_push_head(queue, buffer); 
}


/* 
 * Virtual Methods
 */
static void ufo_resource_manager_dispose(GObject *gobject)
{
    UfoResourceManager *self = UFO_RESOURCE_MANAGER(gobject);

    GList *kernels = g_hash_table_get_values(self->priv->opencl_kernels);
    g_list_foreach(kernels, (gpointer) resource_manager_release_kernel, NULL);
    g_list_foreach(self->priv->opencl_programs, (gpointer) resource_manager_release_program, NULL);

    g_hash_table_destroy(self->priv->buffers);
    g_hash_table_destroy(self->priv->opencl_kernels);
    g_list_free(self->priv->opencl_programs);

    G_OBJECT_CLASS(ufo_resource_manager_parent_class)->dispose(gobject);
}

static void ufo_resource_manager_finalize(GObject *gobject)
{
    UfoResourceManager *self = UFO_RESOURCE_MANAGER(gobject);

    g_free(self->priv->opencl_platforms);
    self->priv->opencl_platforms = NULL;

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
    self->priv = priv = UFO_RESOURCE_MANAGER_GET_PRIVATE(self);
    priv->buffers = g_hash_table_new(NULL, NULL);
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
                CL_DEVICE_TYPE_GPU,
                0, NULL,
                &num_devices);

        priv->opencl_devices[i] = g_malloc0(num_devices * sizeof(cl_device_id));

        clGetDeviceIDs(platform,
                CL_DEVICE_TYPE_GPU,
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
                NULL, NULL, NULL);

        priv->command_queue = clCreateCommandQueue(priv->opencl_context,
                priv->opencl_devices[0][0],
                0, NULL);
        g_debug("Create cmd-queue %p", priv->command_queue);
    }
}
