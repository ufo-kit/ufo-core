/**
 * SECTION:ufo-buffer
 * @Short_description: Represents n-dimensional data
 * @Title: UfoBuffer
 */

#include <string.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "config.h"
#include "ufo-buffer.h"
#include "ufo-resource-manager.h"
#include "ufo-enums.h"

G_DEFINE_TYPE(UfoBuffer, ufo_buffer, G_TYPE_OBJECT);

#define UFO_BUFFER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BUFFER, UfoBufferPrivate))

#define UFO_BUFFER_ERROR ufo_buffer_error_quark()

enum UfoBufferError {
    UFO_BUFFER_ERROR_WRONG_SIZE
};

enum {
    PROP_0,
    PROP_ID,
    PROP_ACCESS,
    PROP_DOMAIN,
    PROP_STRUCTURE,
    N_PROPERTIES
};

static GParamSpec *buffer_properties[N_PROPERTIES] = { NULL, };

typedef enum {
    NO_DATA = 0,
    HOST_ARRAY_VALID,
    DEVICE_ARRAY_VALID
} data_location;

typedef struct {
    guint num_dims;
    gfloat *data;
    guint dim_size[32];
    guint dim_stride[32];
} nd_array;

struct _UfoBufferPrivate {
    nd_array        host_array;
    cl_mem          device_array;
    data_location   location;

    gint        id;     /**< unique id that is passed to the transformed buffer */
    gsize       size;   /**< size of buffer in bytes */

    UfoAccess   access;
    UfoDomain   domain;
    
    cl_event    *events;
    cl_uint     current_event_index; /**< index of currently available event position */
    cl_uint     num_total_events;   /**< total amount of events we can keep */
    
    cl_ulong    time_upload;
    cl_ulong    time_download;
};

GQuark ufo_buffer_error_quark(void)
{
    return g_quark_from_static_string("ufo-buffer-error-quark");
}

/**
 * ufo_buffer_new: 
 * @num_dims: (in): Number of dimensions
 * @dim_size: (in) (array): Size of each dimension
 *
 * Create a new buffer with the given dimensions.
 *
 * Return value: A new #UfoBuffer with the given dimensions.
 */
UfoBuffer *ufo_buffer_new(guint num_dims, const guint *dim_size)
{
    UfoBuffer *buffer = UFO_BUFFER(g_object_new(UFO_TYPE_BUFFER, NULL));
    ufo_buffer_set_dimensions(buffer, num_dims, dim_size);
    return buffer;
}

/**
 * ufo_buffer_set_dimensions:
 * @buffer: A #UfoBuffer
 * @num_dims: (in): Number of dimensions
 * @dim_size: (in) (array): Size of each dimension
 *
 * Specify the size of this nd-array.
 */
void ufo_buffer_set_dimensions(UfoBuffer *buffer, guint num_dims, const guint *dim_size)
{
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE(buffer);
    priv->size = sizeof(float);
    priv->host_array.num_dims = num_dims;

    for (int i = 0; i < num_dims; i++) {
        priv->host_array.dim_size[i] = dim_size[i];
        priv->size *= dim_size[i];
    }

    if (priv->host_array.data) {
        g_free(priv->host_array.data);
        priv->host_array.data = NULL;
    }
}

/**
 * ufo_buffer_get_dimensions:
 * @buffer: A #UfoBuffer
 * @num_dims: (out): Location to store the number of dimensions.
 * @dim_size: (out): Location to store the dimensions. If *dim_size is NULL
 * enough space is allocated to hold num_dims elements and should be freed with
 * g_free(). If *dim_size is NULL, the caller must provide enough memory.
 *
 * Retrieve dimensions of buffer.
 */
void ufo_buffer_get_dimensions(UfoBuffer *buffer, guint *num_dims, guint **dim_size)
{
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE(buffer);

    if ((dim_size == NULL) || (num_dims == NULL))
        return;

    *num_dims = priv->host_array.num_dims;
    if (*dim_size == NULL)
        *dim_size = g_malloc0(sizeof(guint) * (*num_dims));

    g_memmove(*dim_size, priv->host_array.dim_size, sizeof(guint) * (*num_dims));
}

/**
 * ufo_buffer_get_size:
 * @buffer: A #UfoBuffer
 *
 * Get size of internal data in bytes.
 *
 * Return value: Size of data
 */
gsize ufo_buffer_get_size(UfoBuffer *buffer)
{
    return buffer->priv->size;
}

/**
 * ufo_buffer_get_id:
 * @buffer: A #UfoBuffer
 *
 * Get internal identification.
 *
 * Return value: unique and monotone id
 */
gint ufo_buffer_get_id(UfoBuffer *buffer)
{
    return buffer->priv->id;
}

/**
 * ufo_buffer_transfer_id:
 * @from: A #UfoBuffer whose id is copied.
 * @to: UfoBuffer who gets this id
 *
 * Transfer id from one buffer to another.
 */
void ufo_buffer_transfer_id(UfoBuffer *from, UfoBuffer *to)
{
    to->priv->id = from->priv->id;
}

/**
 * ufo_buffer_copy:
 * @from: A #UfoBuffer to copy from.
 * @to: A #UfoBuffer to copy into.
 * @command_queue: A cl_command_queue.
 *
 * Copy the content of one buffer into another.
 */
void ufo_buffer_copy(UfoBuffer *from, UfoBuffer *to, gpointer command_queue)
{
    if (from->priv->location == DEVICE_ARRAY_VALID) {
        UfoResourceManager *manager = ufo_resource_manager();
        to->priv->device_array = ufo_resource_manager_memdup(manager, from->priv->device_array);

        cl_event event;
        CHECK_ERROR(clEnqueueCopyBuffer(command_queue, from->priv->device_array, 
                    to->priv->device_array, 0, 0, from->priv->size, 0, NULL, &event));

        ufo_buffer_attach_event(to, event);
    }
    else if (from->priv->location == HOST_ARRAY_VALID)
        ufo_buffer_set_host_array(to, from->priv->host_array.data, from->priv->size, NULL);
    
    to->priv->location = from->priv->location;
}

/**
 * ufo_buffer_get_2d_dimensions:
 * @buffer: A #UfoBuffer.
 * @width: (out): Location to store the width of the buffer
 * @height: (out): Location to store the height of the buffer
 *
 * Convenience function to retrieve dimension of buffer.
 */
void ufo_buffer_get_2d_dimensions(UfoBuffer *buffer, guint *width, guint *height)
{
    g_return_if_fail(UFO_IS_BUFFER(buffer));
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE(buffer);
    *width = priv->host_array.dim_size[0];
    *height = priv->host_array.dim_size[1];
}

/**
 * ufo_buffer_create_gpu_buffer:
 * @buffer: A #UfoBuffer.
 * @mem: A cl_mem object that the host memory is synchronized with.
 *
 * Associate the buffer with a given OpenCL memory object.  This does not
 * actually copy the data from host to device. Same as ufo_buffer_new(), users
 * should stay away from calling this on their own.
 */
void ufo_buffer_create_gpu_buffer(UfoBuffer *buffer, gpointer mem)
{
    UfoBufferPrivate *priv = buffer->priv;
    if (priv->device_array != NULL)
        clReleaseMemObject(priv->device_array);

    priv->device_array = (cl_mem) mem;
    priv->location = DEVICE_ARRAY_VALID;
}


/**
 * ufo_buffer_invalidate_gpu_data:
 * @buffer: A #UfoBuffer.
 *
 * Invalidate state of a buffer so that Data won't be synchronized between CPU
 * and GPU and must be re-set again with ufo_buffer_set_cpu_data.
 */
void ufo_buffer_invalidate_gpu_data(UfoBuffer *buffer)
{
    buffer->priv->location = HOST_ARRAY_VALID;
}

/**
 * ufo_buffer_reinterpret:
 * @buffer: A #UfoBuffer.
 * @source_depth: The number of bits that make up the original integer data type.  
 * @num_pixels: Number of pixels to consider
 *
 * The fundamental data type of a UfoBuffer is one single-precision floating
 * point per pixel. To increase performance it is possible to load arbitrary
 * integer data with ufo_buffer_set_cpu_data() and convert that data with this
 * method.
 */
void ufo_buffer_reinterpret(UfoBuffer *buffer, gsize source_depth, gsize num_pixels)
{
    float *dst = buffer->priv->host_array.data;
    /* To save a memory allocation and several copies, we process data from back
     * to front. This is possible if src bit depth is at most half as wide as
     * the 32-bit target buffer. The processor cache should not be a problem. */
    if (source_depth == 8) {
        guint8 *src = (guint8 *) buffer->priv->host_array.data;
        for (int index = (num_pixels-1); index >= 0; index--)
            dst[index] = src[index] / 255.0;
    }
    else if (source_depth == 16) {
        guint16 *src = (guint16 *) buffer->priv->host_array.data;
        for (int index = (num_pixels-1); index >= 0; index--)
            dst[index] = src[index] / 65535.0;
    }
}

/**
 * ufo_buffer_set_cl_mem:
 * @buffer: A #UfoBuffer.
 * @mem: A cl_mem object.
 *
 * Set OpenCL memory object that is used to up and download data.
 */
void ufo_buffer_set_cl_mem(UfoBuffer *buffer, gpointer mem)
{
    buffer->priv->device_array = (cl_mem) mem;
}

/**
 * ufo_buffer_get_cl_mem:
 * @buffer: A #UfoBuffer.
 * 
 * Return associated OpenCL memory object without synchronizing with CPU memory.
 *
 * Return value: A cl_mem object associated with this #UfoBuffer.
 */
gpointer ufo_buffer_get_cl_mem(UfoBuffer *buffer)
{
    return buffer->priv->device_array;
}

/**
 * ufo_buffer_attach_event:
 * @buffer: A #UfoBuffer.
 * @event: A cl_event object.
 *
 * Attach an OpenCL event to a buffer that must be finished before anything else
 * can be done with this buffer.
 */
void ufo_buffer_attach_event(UfoBuffer *buffer, gpointer event)
{
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE(buffer);

    priv->events[priv->current_event_index++] = (cl_event) event;

    if (priv->current_event_index == priv->num_total_events) {
        priv->num_total_events *= 2;
        priv->events = g_realloc(priv->events, priv->num_total_events * sizeof(cl_event));
        g_debug("Reallocated event array to %i elements\n", priv->num_total_events);
    }
}

/**
 * ufo_buffer_get_events:
 * @buffer: A #UfoBuffer.
 * @events: (out) (transfer none): Location to store pointer of events.
 * @num_events: (out): Location to store the length of the event array.
 *
 * Return events currently associated with a buffer but don't release them from
 * this buffer.
 */
void ufo_buffer_get_events(UfoBuffer *buffer, gpointer **events, guint *num_events)
{
    *num_events = buffer->priv->current_event_index;
    *events = (gpointer *) buffer->priv->events;
}

/**
 * ufo_buffer_clear_events:
 * @buffer: A #UfoBuffer.
 *
 * Clear and release events associated with a buffer
 */
void ufo_buffer_clear_events(UfoBuffer *buffer)
{
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE(buffer);
    for (int i = 0; i < priv->current_event_index; i++)
        clReleaseEvent(priv->events[i]);
    priv->current_event_index = 0;
}

/**
 * ufo_buffer_get_transfer_time:
 * @buffer: A #UfoBuffer.
 * @upload_time: (out): Location to store the upload time.
 * @download_time: (out): Location to store the download time.
 *
 * Get statistics on how long data was copied to and from GPU devices.
 */
void ufo_buffer_get_transfer_time(UfoBuffer *buffer, gulong *upload_time, gulong *download_time)
{
    *upload_time = buffer->priv->time_upload; 
    *download_time = buffer->priv->time_download; 
}

/**
 * ufo_buffer_set_host_array:
 * @buffer: A #UfoBuffer.
 * @data: User supplied data
 * @num_bytes: Size of data in bytes
 * @error: Pointer to GError*
 *
 * Fill buffer with data. This method does not take ownership of data, it just
 * copies the data off of it because we never know if there is enough memory to
 * hold floats of that data.
 */
void ufo_buffer_set_host_array(UfoBuffer *buffer, float *data, gsize num_bytes, GError **error)
{
    UfoBufferPrivate *priv = buffer->priv;
    if (data == NULL)
        return;

    if (num_bytes > priv->size) {
        if (error != NULL) {
            g_set_error(error,
                    UFO_BUFFER_ERROR,
                    UFO_BUFFER_ERROR_WRONG_SIZE,
                    "Trying to set more data than buffer dimensions allow");
        }
        return;
    }
    if (priv->host_array.data == NULL) 
        priv->host_array.data = g_malloc0(priv->size);

    g_memmove(priv->host_array.data, data, num_bytes);
    priv->location = HOST_ARRAY_VALID;
}

/**
 * ufo_buffer_get_host_array:
 * @buffer: A #UfoBuffer.
 * @command_queue: A cl_command_queue object.
 *
 * Returns a flat C-array containing the raw float data.
 *
 * Return value: Float array.
 */
float* ufo_buffer_get_host_array(UfoBuffer *buffer, gpointer command_queue)
{
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE(buffer);

    switch (priv->location) {
        case HOST_ARRAY_VALID:
            /* This can be the case, when a buffer's device memory was invalidated
             * but no host memory has been allocated before. */
            if (priv->host_array.data == NULL)
                priv->host_array.data = g_malloc0(priv->size);
            break;

        case DEVICE_ARRAY_VALID:
            if (priv->host_array.data == NULL)
                priv->host_array.data = g_malloc0(priv->size);
            
            CHECK_ERROR(clEnqueueReadBuffer(command_queue,
                    priv->device_array,
                    CL_TRUE, 
                    0, priv->size,
                    priv->host_array.data,
                    0, NULL, NULL));

            /* TODO: Can we release the events here? */
            priv->current_event_index = 0;
            priv->location = HOST_ARRAY_VALID;
            break;

        case NO_DATA:
            priv->host_array.data = g_malloc0(priv->size);
            priv->location = HOST_ARRAY_VALID;
            break;
    }

    return priv->host_array.data;
}

/**
 * ufo_buffer_get_device_array:
 * @buffer: A #UfoBuffer
 * @command_queue: A cl_command_queue object that is used to access the device
 * memory.
 *
 * Get OpenCL memory object that is used to up and download data.
 *
 * Return value: OpenCL memory object associated with this #UfoBuffer.
 */
gpointer ufo_buffer_get_device_array(UfoBuffer *buffer, gpointer command_queue)
{
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE(buffer);
    cl_event event;

    switch (priv->location) {
        case HOST_ARRAY_VALID:
            if (priv->device_array == NULL)
                priv->device_array = clCreateBuffer(NULL, CL_MEM_READ_WRITE, priv->size, NULL, NULL);

            CHECK_ERROR(clEnqueueWriteBuffer((cl_command_queue) command_queue,
                    priv->device_array,
                    CL_FALSE,
                    0, priv->size,
                    priv->host_array.data,
                    0, NULL, &event));

            priv->location = DEVICE_ARRAY_VALID;
            break;

        case DEVICE_ARRAY_VALID:
            break;

        case NO_DATA:
            if (priv->device_array) {
                priv->location = DEVICE_ARRAY_VALID;
                break;
            }
            return NULL;
    }
    return priv->device_array;
}

static void ufo_filter_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoBuffer *buffer = UFO_BUFFER(object);

    switch (property_id) {
        case PROP_ID:
            buffer->priv->id = g_value_get_int(value);
            break;
        case PROP_ACCESS:
            buffer->priv->access = g_value_get_flags(value);
            break;
        case PROP_DOMAIN:
            buffer->priv->domain = g_value_get_enum(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoBuffer *buffer = UFO_BUFFER(object);

    switch (property_id) {
        case PROP_ID:
            g_value_set_int(value, buffer->priv->id);
            break;
        case PROP_ACCESS:
            g_value_set_flags(value, buffer->priv->access);
            break;
        case PROP_DOMAIN:
            g_value_set_enum(value, buffer->priv->domain);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_buffer_finalize(GObject *gobject)
{
    UfoBuffer *buffer = UFO_BUFFER(gobject);
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE(buffer);
    if (priv->host_array.data)
        g_free(priv->host_array.data);
    
    priv->host_array.data = NULL;
    g_free(priv->events);
    
    G_OBJECT_CLASS(ufo_buffer_parent_class)->finalize(gobject);
}

static void ufo_buffer_class_init(UfoBufferClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = ufo_filter_set_property;
    gobject_class->get_property = ufo_filter_get_property;
    gobject_class->finalize = ufo_buffer_finalize;

    buffer_properties[PROP_ID] = 
        g_param_spec_int("id",
            "ID of this buffer",
            "ID of this buffer",
            -1,
            32736,
            0,
            G_PARAM_READWRITE);

    buffer_properties[PROP_ACCESS] = 
        g_param_spec_flags("access",
            "Access restrictions",
            "Read/Write access restrictions of this buffer",
            UFO_TYPE_ACCESS,
            UFO_BUFFER_READWRITE,
            G_PARAM_READABLE);

    buffer_properties[PROP_DOMAIN] = 
        g_param_spec_enum("domain",
            "Domain of the buffer",
            "Domain of the buffer, either spatial or frequential",
            UFO_TYPE_DOMAIN,
            UFO_BUFFER_SPACE,
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_ID, buffer_properties[PROP_ID]);
    g_object_class_install_property(gobject_class, PROP_ACCESS, buffer_properties[PROP_ACCESS]);
    g_object_class_install_property(gobject_class, PROP_DOMAIN, buffer_properties[PROP_DOMAIN]);

    g_type_class_add_private(klass, sizeof(UfoBufferPrivate));
}

static void ufo_buffer_init(UfoBuffer *buffer)
{
    UfoBufferPrivate *priv;
    buffer->priv = priv = UFO_BUFFER_GET_PRIVATE(buffer);
    priv->id = -1;

    priv->device_array = NULL;
    priv->host_array.num_dims = 0;
    priv->host_array.data = NULL;
    priv->location = NO_DATA;

    priv->current_event_index = 0;
    priv->num_total_events = 8;
    priv->events = g_malloc0(priv->num_total_events * sizeof(cl_event));

    priv->time_upload = 0;
    priv->time_download = 0;
}
