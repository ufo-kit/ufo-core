#include <string.h>
#include <CL/cl.h>

#include "config.h"
#include "ufo-buffer.h"

G_DEFINE_TYPE(UfoBuffer, ufo_buffer, G_TYPE_OBJECT);

#define UFO_BUFFER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BUFFER, UfoBufferPrivate))

#define UFO_BUFFER_ERROR ufo_buffer_error_quark()

enum UfoBufferError {
    UFO_BUFFER_ERROR_WRONG_SIZE
};

enum {
    PROP_0,
    PROP_ID,
    PROP_FINISHED,
    PROP_ACCESS,
    N_PROPERTIES
};

static GParamSpec *buffer_properties[N_PROPERTIES] = { NULL, };

typedef enum {
    NO_DATA = 0,
    CPU_DATA_VALID,
    GPU_DATA_VALID
} datastates;

struct _UfoBufferPrivate {
    gint32      width;
    gint32      height;
    gsize       size;   /**< size of buffer in bytes */
    gint        uploads;
    gint        downloads;
    gint        access;
    gint        id;     /**< unique id that is passed to the transformed buffer */

    gboolean    finished;
    datastates  state;
    float       *cpu_data;
    cl_mem      gpu_data;
    GQueue      *wait_events;
    
    cl_ulong    time_upload;
    cl_ulong    time_download;
};

static void buffer_set_dimensions(UfoBufferPrivate *priv, gint32 width, gint32 height)
{
    priv->width = width;
    priv->height = height;
    priv->size = width * height * sizeof(float);
}

GQuark ufo_buffer_error_quark(void)
{
    return g_quark_from_static_string("ufo-buffer-error-quark");
}

/**
 * \brief Converts the queue of events to a flat C array
 * \param[in] priv Private data of a UfoBuffer
 * \param[out] events Pointer to uninitialized array
 * \param[out] num_events Number of events in *events
 * \note You have to free the *events array using g_free().
 */
static void buffer_get_wait_events(UfoBufferPrivate *priv, cl_event** events, guint *num_events)
{
    const guint n = g_queue_get_length(priv->wait_events);
    *num_events = n;
    *events = (cl_event*) g_malloc0(sizeof(cl_event) * n);
    for (int i = 0; i < n; i++)
        *events[i] = g_queue_pop_head(priv->wait_events);
    g_assert(g_queue_get_length(priv->wait_events) == 0);
}

/* 
 * Public Interface
 */

/**
 * \brief Create a new buffer with given dimensions
 * \public \memberof UfoBuffer
 *
 * \param[in] width Width of the two-dimensional buffer
 * \param[in] height Height of the two-dimensional buffer
 *
 * \return Buffer with allocated memory
 *
 * \note Filters should never allocate buffers on their own using this method
 * but use the UfoResourceManager method ufo_resource_manager_request_buffer().
 */
UfoBuffer *ufo_buffer_new(gint32 width, gint32 height)
{
    UfoBuffer *buffer = UFO_BUFFER(g_object_new(UFO_TYPE_BUFFER, NULL));
    buffer_set_dimensions(buffer->priv, width, height);
    return buffer;
}

void ufo_buffer_transfer_id(UfoBuffer *from, UfoBuffer *to)
{
    gint id;
    g_object_get(from, "id", &id, NULL);
    g_object_set(to, "id", id, NULL);
}

UfoBuffer *ufo_buffer_copy(UfoBuffer *buffer, gpointer command_queue)
{
    UfoBuffer *copy = UFO_BUFFER(g_object_new(UFO_TYPE_BUFFER, NULL));

    buffer_set_dimensions(copy->priv, buffer->priv->width, buffer->priv->height);
    g_assert(copy->priv->cpu_data == NULL);
    ufo_buffer_set_cpu_data(copy,
            ufo_buffer_get_cpu_data(buffer, command_queue),
            copy->priv->size,
            NULL);
    if (!ufo_buffer_is_finished(buffer))
        g_message("copy=%p src=%f dst=%f", copy, buffer->priv->cpu_data[0], copy->priv->cpu_data[0]);

    copy->priv->finished = buffer->priv->finished;
    copy->priv->state = CPU_DATA_VALID;
    return copy;
}


/**
 * \brief Retrieve dimension of buffer
 * \public \memberof UfoBuffer
 *
 * \param[in] buffer A UfoBuffer
 * \param[out] width Width of the buffer
 * \param[out] height Height of the buffer
 */
void ufo_buffer_get_dimensions(UfoBuffer *buffer, gint32 *width, gint32 *height)
{
    g_return_if_fail(UFO_IS_BUFFER(buffer));
    *width = buffer->priv->width;
    *height = buffer->priv->height;
}

/**
 * \brief Associate the buffer with a given OpenCL memory object
 * \public \memberof UfoBuffer
 *
 * \param[in] buffer A UfoBuffer
 * \param[in] mem The OpenCL memory object, that the host memory is synchronized
 *      with
 *
 * \note This does not actually copy the data from host to device. Same as
 *      ufo_buffer_new(), users should stay away from calling this on their own.
 */
void ufo_buffer_create_gpu_buffer(UfoBuffer *buffer, gpointer mem)
{
    buffer->priv->gpu_data = (cl_mem) mem;
}

/**
 * \brief Fill buffer with data
 * \public \memberof UfoBuffer
 *
 * This method does not take the ownership of data, it just copies the data off
 * of it because we never know if there is enough memory to hold floats of that
 * data.
 *
 * \param[in] buffer A UfoBuffer to fill the data with
 * \param[in] data User supplied data
 * \param[in] n Size of data in bytes
 * \param[in] error Pointer to GError*
 */
void ufo_buffer_set_cpu_data(UfoBuffer *buffer, float *data, gsize n, GError **error)
{
    if (data == NULL)
        return;

    const gsize num_bytes = buffer->priv->width * buffer->priv->height * sizeof(float);
    if (n > num_bytes) {
        if (error != NULL) {
            g_set_error(error,
                    UFO_BUFFER_ERROR,
                    UFO_BUFFER_ERROR_WRONG_SIZE,
                    "Trying to set more data than buffer dimensions allow");
        }
        return;
    }
    if (buffer->priv->cpu_data == NULL) {
        buffer->priv->cpu_data = g_malloc0(num_bytes);
    }

    memcpy(buffer->priv->cpu_data, data, n);
    buffer->priv->state = CPU_DATA_VALID;
}

/**
 * \brief Spread raw data without increasing the contrast
 * \public \memberof UfoBuffer
 *
 * The fundamental data type of a UfoBuffer is one single-precision floating
 * point per pixel. To increase performance it is possible to load arbitrary
 * integer data with ufo_buffer_set_cpu_data() and convert that data with this
 * method.
 *
 * \param[in] buffer UfoBuffer object
 * \param[in] source_depth The number of bits that make up the original integer data type.  
 * \param[in] n Number of data elements to consider
 */
void ufo_buffer_reinterpret(UfoBuffer *buffer, gsize source_depth, gsize n)
{
    float *dst = buffer->priv->cpu_data;
    /* To save a memory allocation and several copies, we process data from back
     * to front. This is possible if src bit depth is at most half as wide as
     * the 32-bit target buffer. The processor cache should not be a problem. */
    if (source_depth == 8) {
        guint8 *src = (guint8 *) buffer->priv->cpu_data;
        for (int index = (n-1); index >= 0; index--)
            dst[index] = src[index] / 255.0;
    }
    else if (source_depth == 16) {
        guint16 *src = (guint16 *) buffer->priv->cpu_data;
        for (int index = (n-1); index >= 0; index--)
            dst[index] = src[index] / 65535.0;
    }
}

/**
 * \brief Set OpenCL memory object that is used to up and download data.
 * \public \memberof UfoBuffer
 *
 * \param[in] buffer UfoBuffer object
 * \param[in] mem OpenCL memory object to be associated with this UfoBuffer
 */
void ufo_buffer_set_cl_mem(UfoBuffer *buffer, gpointer mem)
{
    buffer->priv->gpu_data = (cl_mem) mem;
}

/**
 * \brief Add a cl_event object on which the buffer has to wait when up or
 * downloading data to or from the GPU.
 * \public \memberof UfoBuffer
 *
 * \param[in] buffer A UfoBuffer
 * \param[in] event A cl_event object
 */
void ufo_buffer_wait_on_event(UfoBuffer *buffer, gpointer event)
{
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE(buffer);
    /* FIXME: does this need to be thread-safe? */
    g_queue_push_tail(priv->wait_events, event);
}

/**
 * \brief Get statistics on how often data was copied to and from GPU devices
 * \param[in] buffer A UfoBuffer
 * \param[out] uploads Number of transfers to GPU
 * \param[out] downloads Number of transfers from GPU
 */
void ufo_buffer_get_transfer_statistics(UfoBuffer *buffer, gint *uploads, gint *downloads)
{
    *uploads = buffer->priv->uploads;
    *downloads = buffer->priv->downloads;
}

void ufo_buffer_get_transfer_time(UfoBuffer *buffer, gulong *upload_time, gulong *download_time)
{
    *upload_time = buffer->priv->time_upload; 
    *download_time = buffer->priv->time_download; 
}

/*
 * \brief Get raw pixel data in a flat array (row-column format)
 * \public \memberof UfoBuffer
 *
 * \param[in] buffer UfoBuffer object
 * \return Pointer to a float array of valid data
 */
float* ufo_buffer_get_cpu_data(UfoBuffer *buffer, gpointer command_queue)
{
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE(buffer);
    cl_event *wait_events = NULL, event;
    cl_uint num_events;

#ifdef WITH_PROFILING
    cl_ulong start, end;
#endif

    switch (priv->state) {
        case CPU_DATA_VALID:
            break;
        case GPU_DATA_VALID:
            if (priv->cpu_data == NULL)
                priv->cpu_data = g_malloc0(priv->size);
            else
                memset(priv->cpu_data, 0, priv->size);

            buffer_get_wait_events(priv, &wait_events, &num_events);
            clWaitForEvents(num_events, wait_events);
            clEnqueueReadBuffer((cl_command_queue) command_queue,
                                priv->gpu_data,
                                CL_TRUE, 
                                0, priv->size,
                                priv->cpu_data,
                                0, NULL, &event);

            clWaitForEvents(1, &event);
#ifdef WITH_PROFILING
            clWaitForEvents(1, &event);
            clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
            clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
            priv->time_download += end - start;
#endif

            /* TODO: we should clear all events from the wait_queue */
            g_free(wait_events);
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
 * \brief Get OpenCL memory object that is used to up and download data.
 * \public \memberof UfoBuffer
 *
 * \param[in] buffer UfoBuffer object
 * \return OpenCL memory object associated with this UfoBuffer
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
            clEnqueueWriteBuffer((cl_command_queue) command_queue,
                                 priv->gpu_data,
                                 CL_FALSE,
                                 0, priv->size,
                                 priv->cpu_data,
                                 0, NULL, &event);

#ifdef WITH_PROFILING
            clWaitForEvents(1, &event);
            clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
            clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
            priv->time_upload += end - start;
#endif
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

/**
 * \brief Return if buffer is denotes finishing of computation
 * \public \memberof UfoBuffer
 *
 * \param[in] buffer A UfoBuffer
 * \return TRUE if buffer just denotes that end of computation is reached, else
 *      FALSE
 */
gboolean ufo_buffer_is_finished(UfoBuffer *buffer)
{
    gboolean finished = FALSE;
    g_object_get(buffer, "finished", &finished, NULL);
    return finished;
}

/* 
 * Virtual Methods 
 */
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
        case PROP_FINISHED:
            buffer->priv->finished = g_value_get_boolean(value);
            break;
        case PROP_ACCESS:
            buffer->priv->access = g_value_get_flags(value);
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
        case PROP_FINISHED:
            g_value_set_boolean(value, buffer->priv->finished);
            break;
        case PROP_ACCESS:
            g_value_set_flags(value, buffer->priv->access);
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
    g_queue_free(priv->wait_events);

    G_OBJECT_CLASS(ufo_buffer_parent_class)->finalize(gobject);
}

#define UFO_BUFFER_TYPE_FLAGS (ufo_buffer_flags_gettype())
static GType ufo_buffer_flags_gettype()
{
    static GType flags_type = 0;
    if (!flags_type) {
        static const GFlagsValue flags[] = {
            { UFO_BUFFER_READABLE, "Read only buffer", "readonly" },
            { UFO_BUFFER_WRITEABLE, "Write only buffer", "writeonly" },
            { UFO_BUFFER_READWRITE, "Read and write access", "readwrite" },
            { 0, NULL, NULL },
        };
        flags_type = g_flags_register_static("UfoBufferFlags", flags);
    }
    return flags_type;
}

/*
 * Type/Class Initialization
 */
static void ufo_buffer_class_init(UfoBufferClass *klass)
{
    /* override methods */
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = ufo_filter_set_property;
    gobject_class->get_property = ufo_filter_get_property;
    gobject_class->finalize = ufo_buffer_finalize;

    /* install properties */
    buffer_properties[PROP_ID] = 
        g_param_spec_int("id",
            "ID of this buffer",
            "ID of this buffer",
            -1,
            8192,
            0,
            G_PARAM_READWRITE);

    buffer_properties[PROP_FINISHED] = 
        g_param_spec_boolean("finished",
            "Is last buffer with invalid data and no one follows",
            "Get finished prop",
            FALSE,
            G_PARAM_READWRITE);

    buffer_properties[PROP_ACCESS] = 
        g_param_spec_flags("access",
            "Access restrictions",
            "Read/Write access restrictions of this buffer",
            UFO_BUFFER_TYPE_FLAGS,
            UFO_BUFFER_READWRITE,
            G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_ID, buffer_properties[PROP_ID]);
    g_object_class_install_property(gobject_class, PROP_FINISHED, buffer_properties[PROP_FINISHED]);
    g_object_class_install_property(gobject_class, PROP_ACCESS, buffer_properties[PROP_ACCESS]);

    /* TODO: use this when updating to GObject 2.26
    g_object_class_install_properties(gobject_class,
            N_PROPERTIES,
            buffer_properties);
    */

    /* install private data */
    g_type_class_add_private(klass, sizeof(UfoBufferPrivate));
}

static void ufo_buffer_init(UfoBuffer *buffer)
{
    UfoBufferPrivate *priv;
    buffer->priv = priv = UFO_BUFFER_GET_PRIVATE(buffer);
    priv->width = -1;
    priv->height = -1;
    priv->cpu_data = NULL;
    priv->gpu_data = NULL;
    priv->uploads = 0;
    priv->downloads = 0;
    priv->id = -1;
    priv->state = NO_DATA;
    priv->finished = FALSE;
    priv->wait_events = g_queue_new();
    priv->time_upload = 0;
    priv->time_download = 0;
}
