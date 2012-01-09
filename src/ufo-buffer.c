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
    CPU_DATA_VALID,
    GPU_DATA_VALID
} datastates;

struct _UfoBufferPrivate {
    gint32      dimensions[4];
    gsize       size;   /**< size of buffer in bytes */
    gint        uploads;
    gint        downloads;
    gint        id;     /**< unique id that is passed to the transformed buffer */

    UfoAccess   access;
    UfoDomain   domain;
    UfoStructure structure;
    
    datastates  state;
    float       *cpu_data;
    cl_mem      gpu_data;

    cl_event    *events;
    cl_uint     current_event_index; /**< index of currently available event position */
    cl_uint     num_total_events;   /**< total amount of events we can keep */
    
    cl_ulong    time_upload;
    cl_ulong    time_download;
};

static void buffer_set_dimensions(UfoBufferPrivate *priv, UfoStructure structure, const gint32 dimensions[4])
{
    int num_elements = 1;
    for (int i = 0; i < 4; i++) {
        priv->dimensions[i] = dimensions[i];
        num_elements *= dimensions[i];
    }
    priv->structure = structure;
    priv->size = num_elements * sizeof(float);
}

GQuark ufo_buffer_error_quark(void)
{
    return g_quark_from_static_string("ufo-buffer-error-quark");
}

/**
 * ufo_buffer_new:
 * @structure: The type of this buffer.
 * @dimensions: 4-d array containing length of each dimension
 *
 * Filters should never allocate buffers on their own using this method
 * but use the UfoResourceManager method ufo_resource_manager_request_buffer()
 * or ufo_channel_allocate_output_buffers() for outgoing data.
 *
 * Return value: A #UfoBuffer with allocated memory.
 */
UfoBuffer *ufo_buffer_new(UfoStructure structure, const gint32 dimensions[4])
{
    UfoBuffer *buffer = UFO_BUFFER(g_object_new(UFO_TYPE_BUFFER, NULL));
    buffer_set_dimensions(buffer->priv, structure, dimensions);
    return buffer;
}

/**
 * ufo_buffer_get_size:
 * @buffer: A #UfoBuffer.
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
 * @buffer: A #UfoBuffer.
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
    UfoResourceManager *manager = ufo_resource_manager();
    if (from->priv->state == GPU_DATA_VALID) {
        to->priv->gpu_data = ufo_resource_manager_memdup(manager, from->priv->gpu_data);

        cl_event event;
        CHECK_ERROR(clEnqueueCopyBuffer(command_queue, from->priv->gpu_data, 
                    to->priv->gpu_data, 0, 0, from->priv->size, 0, NULL, &event));
        ufo_buffer_attach_event(to, event);
    }
    else if (from->priv->state == CPU_DATA_VALID)
        ufo_buffer_set_cpu_data(to, from->priv->cpu_data, from->priv->size, NULL);
    
    to->priv->state = from->priv->state;
}


/**
 * ufo_buffer_get_dimensions:
 * @buffer: A #UfoBuffer.
 * @dimensions: (array): array with four elements
 *
 * Retrieve dimension of buffer.
 */
void ufo_buffer_get_dimensions(UfoBuffer *buffer, gint32 *dimensions)
{
    g_return_if_fail(UFO_IS_BUFFER(buffer));
    gint32 *dims = buffer->priv->dimensions;
    for (int i = 0; i < 4; i++)
        dimensions[i] = dims[i];
}

/**
 * ufo_buffer_get_2d_dimensions:
 * @buffer: A #UfoBuffer.
 * @width: (out): Location to store the width of the buffer
 * @height: (out): Location to store the height of the buffer
 *
 * Convenience function to retrieve dimension of buffer.
 */
void ufo_buffer_get_2d_dimensions(UfoBuffer *buffer, gint32 *width, gint32 *height)
{
    g_return_if_fail(UFO_IS_BUFFER(buffer));
    gint32 *dims = buffer->priv->dimensions;
    *width = dims[0];
    *height = dims[1];
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
    if (priv->gpu_data != NULL)
        clReleaseMemObject(priv->gpu_data);

    priv->gpu_data = (cl_mem) mem;
    priv->state = GPU_DATA_VALID;
}

/**
 * ufo_buffer_set_cpu_data:
 * @buffer: A #UfoBuffer.
 * @data: User supplied data
 * @num_bytes: Size of data in bytes
 * @error: Pointer to GError*
 *
 * Fill buffer with data. This method does not take ownership of data, it just
 * copies the data off of it because we never know if there is enough memory to
 * hold floats of that data.
 */
void ufo_buffer_set_cpu_data(UfoBuffer *buffer, float *data, gsize num_bytes, GError **error)
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
    if (priv->cpu_data == NULL) 
        priv->cpu_data = g_malloc0(priv->size);

    memcpy(priv->cpu_data, data, num_bytes);
    priv->state = CPU_DATA_VALID;
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
    buffer->priv->state = CPU_DATA_VALID;
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
    float *dst = buffer->priv->cpu_data;
    /* To save a memory allocation and several copies, we process data from back
     * to front. This is possible if src bit depth is at most half as wide as
     * the 32-bit target buffer. The processor cache should not be a problem. */
    if (source_depth == 8) {
        guint8 *src = (guint8 *) buffer->priv->cpu_data;
        for (int index = (num_pixels-1); index >= 0; index--)
            dst[index] = src[index] / 255.0;
    }
    else if (source_depth == 16) {
        guint16 *src = (guint16 *) buffer->priv->cpu_data;
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
    buffer->priv->gpu_data = (cl_mem) mem;
}

/**
 * ufo_buffer_get_cl_mem:
 * @buffer: A #UfoBuffer.
 * 
 * Return associated OpenCL memory object without synchronizing with CPU memory.
 *
 * Return value: A cl_mem object associated with this #UfoBuffer.
 */
void *ufo_buffer_get_cl_mem(UfoBuffer *buffer)
{
    return buffer->priv->gpu_data;
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
 * ufo_buffer_get_cpu_data:
 * @buffer: A #UfoBuffer.
 * @command_queue: A cl_command_queue object that is used to access the device
 * memory.
 *
 * Get raw pixel data in a flat array (row-column format).
 *
 * Return value: (transfer none): Pointer to a float array of valid data.
 */
float* ufo_buffer_get_cpu_data(UfoBuffer *buffer, gpointer command_queue)
{
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE(buffer);
    cl_event event;
#ifdef WITH_PROFILING
    cl_ulong start, end;
#endif

    switch (priv->state) {
        case CPU_DATA_VALID:
            /* This can be the case, when a buffer's device memory was invalidated
             * but no host memory has been allocated before. */
            if (priv->cpu_data == NULL)
                priv->cpu_data = g_malloc0(priv->size);
            break;
        case GPU_DATA_VALID:
            if (priv->cpu_data == NULL)
                priv->cpu_data = g_malloc0(priv->size);
            
            if (command_queue == NULL)
                return NULL;

            CHECK_ERROR(clEnqueueReadBuffer(command_queue,
                    priv->gpu_data,
                    CL_TRUE, 
                    0, priv->size,
                    priv->cpu_data,
                    0, NULL, NULL));
                    /* priv->current_event_index, priv->events, &event)); */

            /* TODO: Can we release the events here? */
            ufo_buffer_clear_events(buffer);
            priv->current_event_index = 0;

#ifdef WITH_PROFILING
            clWaitForEvents(1, &event);
            clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
            clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
            priv->time_download += end - start;
#endif

            priv->state = CPU_DATA_VALID;
            priv->downloads++;
            break;
        case NO_DATA:
            priv->cpu_data = g_malloc0(priv->size);
            priv->state = CPU_DATA_VALID;
            break;
    }

    return priv->cpu_data;
}


/**
 * ufo_buffer_get_gpu_data:
 * @buffer: A #UfoBuffer
 * @command_queue: A cl_command_queue object that is used to access the device
 * memory.
 *
 * Get OpenCL memory object that is used to up and download data.
 *
 * Return value: OpenCL memory object associated with this #UfoBuffer.
 */
gpointer ufo_buffer_get_gpu_data(UfoBuffer *buffer, gpointer command_queue)
{
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE(buffer);
    cl_event event;
#ifdef WITH_PROFILING
    cl_ulong start, end;
#endif

    switch (priv->state) {
        case CPU_DATA_VALID:
            if (priv->gpu_data == NULL)
                priv->gpu_data = ufo_resource_manager_memalloc(ufo_resource_manager(), priv->size);

            CHECK_ERROR(clEnqueueWriteBuffer((cl_command_queue) command_queue,
                    priv->gpu_data,
                    CL_FALSE,
                    0, priv->size,
                    priv->cpu_data,
                    0, NULL, &event));

#ifdef WITH_PROFILING
            clWaitForEvents(1, &event);
            clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
            clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
            priv->time_upload += end - start;
#endif
            ufo_buffer_attach_event(buffer, event);
            /* if (event) */
            /*     CHECK_ERROR(clReleaseEvent(event)); */

            priv->state = GPU_DATA_VALID;
            priv->uploads++;
            break;
        case GPU_DATA_VALID:
            break;
        case NO_DATA:
            if (priv->gpu_data) {
                priv->state = GPU_DATA_VALID;
                break;
            }
            return NULL;
    }
    return priv->gpu_data;
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
        case PROP_STRUCTURE:
            buffer->priv->structure = g_value_get_enum(value);
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
        case PROP_STRUCTURE:
            g_value_set_enum(value, buffer->priv->structure);
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
    if (priv->cpu_data)
        g_free(priv->cpu_data);
    
    priv->cpu_data = NULL;
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

    buffer_properties[PROP_STRUCTURE] = 
        g_param_spec_enum("structure",
            "Spatial structure of the buffer",
            "Spatial structure of the buffer",
            UFO_TYPE_STRUCTURE,
            UFO_BUFFER_2D,
            G_PARAM_READWRITE);

    buffer_properties[PROP_DOMAIN] = 
        g_param_spec_enum("domain",
            "Domain of the buffer",
            "Domain of the buffer, either spatial or frequential",
            UFO_TYPE_DOMAIN,
            UFO_BUFFER_SPACE,
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_ID, buffer_properties[PROP_ID]);
    g_object_class_install_property(gobject_class, PROP_ACCESS, buffer_properties[PROP_ACCESS]);
    g_object_class_install_property(gobject_class, PROP_STRUCTURE, buffer_properties[PROP_STRUCTURE]);
    g_object_class_install_property(gobject_class, PROP_DOMAIN, buffer_properties[PROP_DOMAIN]);

    g_type_class_add_private(klass, sizeof(UfoBufferPrivate));
}

static void ufo_buffer_init(UfoBuffer *buffer)
{
    UfoBufferPrivate *priv;
    buffer->priv = priv = UFO_BUFFER_GET_PRIVATE(buffer);
    priv->dimensions[0] = priv->dimensions[1] = priv->dimensions[2] = priv->dimensions[3] = 1;
    priv->cpu_data = NULL;
    priv->gpu_data = NULL;
    priv->uploads = 0;
    priv->downloads = 0;
    priv->id = -1;
    priv->state = NO_DATA;
    priv->access = UFO_BUFFER_READWRITE;
    priv->domain = UFO_BUFFER_SPACE;
    priv->structure = UFO_BUFFER_2D;

    priv->current_event_index = 0;
    priv->num_total_events = 8;
    priv->events = g_malloc0(priv->num_total_events * sizeof(cl_event));

    priv->time_upload = 0;
    priv->time_download = 0;
}
