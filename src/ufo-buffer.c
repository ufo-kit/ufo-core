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
#include "ufo-aux.h"
#include "ufo-resource-manager.h"
#include "ufo-enums.h"

G_DEFINE_TYPE(UfoBuffer, ufo_buffer, G_TYPE_OBJECT)

#define UFO_BUFFER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BUFFER, UfoBufferPrivate))

#define UFO_BUFFER_ERROR ufo_buffer_error_quark()


enum {
    PROP_0,
    PROP_ID,
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
    guint dim_size[UFO_BUFFER_MAX_NDIMS];
    guint dim_stride[UFO_BUFFER_MAX_NDIMS];
} nd_array;

struct _UfoBufferPrivate {
    nd_array        host_array;
    cl_mem          device_array;
    data_location   location;

    gint        id;     /**< unique id that is passed to the transformed buffer */
    gsize       size;   /**< size of buffer in bytes */

    cl_event    *events;
    cl_uint     current_event_index; /**< index of currently available event position */
    cl_uint     num_total_events;   /**< total amount of events we can keep */

    GTimer      *timer;     /**< holds elapsed time for up/download */
    cl_ulong    time_upload;
    cl_ulong    time_download;
};

/**
 * UfoBufferError:
 * @UFO_BUFFER_ERROR_WRONG_SIZE: Wrongly sized array passed to the buffer
 *
 * Possible errors when using a UfoBuffer.
 */
GQuark ufo_buffer_error_quark(void)
{
    return g_quark_from_static_string("ufo-buffer-error-quark");
}

static void
ufo_buffer_set_dimensions(UfoBuffer *buffer, guint num_dims, const guint *dim_size)
{
    g_return_if_fail (UFO_IS_BUFFER(buffer));
    g_return_if_fail ((num_dims > 0) && (num_dims <= UFO_BUFFER_MAX_NDIMS));
    g_return_if_fail (dim_size != NULL);

    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE(buffer);
    priv->size = sizeof(gfloat);
    priv->host_array.num_dims = num_dims;

    for (guint i = 0; i < num_dims; i++) {
        priv->host_array.dim_size[i] = dim_size[i];
        priv->size *= dim_size[i];
    }
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
UfoBuffer *
ufo_buffer_new (guint num_dims, const guint *dim_size)
{
    g_return_val_if_fail ((num_dims <= UFO_BUFFER_MAX_NDIMS) && (dim_size != NULL), NULL);
    UfoBuffer *buffer = UFO_BUFFER (g_object_new (UFO_TYPE_BUFFER, NULL));
    ufo_buffer_set_dimensions (buffer, num_dims, dim_size);
    return buffer;
}

/**
 * ufo_buffer_alloc_host_mem:
 * @buffer: A #UfoBuffer object
 *
 * Allocate host memory for dimensions specified in ufo_buffer_new().
 *
 * Since: 0.2
 */
void
ufo_buffer_alloc_host_mem (UfoBuffer *buffer)
{
    UfoBufferPrivate *priv;

    g_return_if_fail (UFO_IS_BUFFER (buffer));
    priv = UFO_BUFFER_GET_PRIVATE (buffer);

    if (priv->host_array.data != NULL)
        g_free (priv->host_array.data);

    priv->host_array.data = g_malloc0(priv->size);
}

/**
 * ufo_buffer_copy:
 * @src: Source #UfoBuffer
 * @dst: Destination #UfoBuffer
 *
 * Copy the contents of %src into %dst.
 *
 * Since: 0.2
 */
void
ufo_buffer_copy (UfoBuffer *src, UfoBuffer *dst)
{
    g_return_if_fail (UFO_IS_BUFFER (src) && UFO_IS_BUFFER (dst));
    g_return_if_fail (src->priv->size == dst->priv->size);

    if (src->priv->location != HOST_ARRAY_VALID) {
        g_warning ("Copying source buffers with invalid or device memory not yet supported");
        return;
    }

    if (dst->priv->host_array.data == NULL)
        ufo_buffer_alloc_host_mem (dst);

    g_memmove (dst->priv->host_array.data, src->priv->host_array.data, src->priv->size);
    dst->priv->location = HOST_ARRAY_VALID;
}

/**
 * ufo_buffer_resize:
 * @buffer: A #UfoBuffer
 * @num_dims: New number of dimensions
 * @dim_size: New dimension sizes
 *
 * Resize an existing buffer.
 * Since: 0.2
 */
void
ufo_buffer_resize (UfoBuffer *buffer, guint num_dims, const guint *dim_size)
{
    UfoBufferPrivate *priv;

    g_return_if_fail ((num_dims <= UFO_BUFFER_MAX_NDIMS) && (dim_size != NULL));

    ufo_buffer_set_dimensions (buffer, num_dims, dim_size);
    priv = UFO_BUFFER_GET_PRIVATE (buffer);

    if (priv->host_array.data != NULL) {
        g_free (priv->host_array.data);
        priv->host_array.data = NULL;
    }

    if (priv->device_array != NULL) {
        CHECK_OPENCL_ERROR (clReleaseMemObject (priv->device_array));
        priv->device_array = NULL;
    }
}

/**
 * ufo_buffer_get_dimensions:
 * @buffer: A #UfoBuffer
 * @num_dims: (out): Location to store the number of dimensions.
 * @dim_size: (out): Location to store the dimensions. If *dim_size is NULL
 *      enough space is allocated to hold num_dims elements and should be freed
 *      with g_free(). If *dim_size is NULL, the caller must provide enough
 *      memory.
 *
 * Retrieve dimensions of buffer.
 */
void
ufo_buffer_get_dimensions(UfoBuffer *buffer, guint *num_dims, guint **dim_size)
{
    g_return_if_fail(UFO_IS_BUFFER(buffer) || (num_dims == NULL) || (dim_size == NULL));
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE(buffer);

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
gsize
ufo_buffer_get_size(UfoBuffer *buffer)
{
    g_return_val_if_fail(UFO_IS_BUFFER(buffer), 0);
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
gint
ufo_buffer_get_id(UfoBuffer *buffer)
{
    g_return_val_if_fail(UFO_IS_BUFFER(buffer), -1);
    return buffer->priv->id;
}

/**
 * ufo_buffer_transfer_id:
 * @from: A #UfoBuffer whose id is copied.
 * @to: UfoBuffer who gets this id
 *
 * Transfer id from one buffer to another.
 */
void
ufo_buffer_transfer_id(UfoBuffer *from, UfoBuffer *to)
{
    g_return_if_fail(UFO_IS_BUFFER(from) || (UFO_IS_BUFFER(to)));
    to->priv->id = from->priv->id;
}

/**
 * ufo_buffer_get_2d_dimensions:
 * @buffer: A #UfoBuffer.
 * @width: (out): Location to store the width of the buffer
 * @height: (out): Location to store the height of the buffer
 *
 * Convenience function to retrieve dimension of buffer.
 */
void
ufo_buffer_get_2d_dimensions(UfoBuffer *buffer, guint *width, guint *height)
{
    g_return_if_fail(UFO_IS_BUFFER(buffer) || (width != NULL) || (height != NULL));
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE(buffer);
    *width = priv->host_array.dim_size[0];
    *height = priv->host_array.dim_size[1];
}

/**
 * ufo_buffer_invalidate_gpu_data:
 * @buffer: A #UfoBuffer.
 *
 * Invalidate state of a buffer so that Data won't be synchronized between CPU
 * and GPU and must be re-set again with ufo_buffer_set_cpu_data.
 */
void
ufo_buffer_invalidate_gpu_data(UfoBuffer *buffer)
{
    g_return_if_fail(UFO_IS_BUFFER(buffer));
    buffer->priv->location = HOST_ARRAY_VALID;
}

/**
 * ufo_buffer_reinterpret:
 * @buffer: A #UfoBuffer.
 * @source_depth: The number of bits that make up the original integer data type.
 * @num_pixels: Number of pixels to consider
 * @normalize: Normalize image data to range [0.0, 1.0]
 *
 * The fundamental data type of a UfoBuffer is one single-precision floating
 * point per pixel. To increase performance it is possible to load arbitrary
 * integer data with ufo_buffer_set_cpu_data() and convert that data with this
 * method.
 */
void
ufo_buffer_reinterpret(UfoBuffer *buffer, gsize source_depth, gsize num_pixels, gboolean normalize)
{
    g_return_if_fail (UFO_IS_BUFFER(buffer));
    gfloat *dst = buffer->priv->host_array.data;

    /* To save a memory allocation and several copies, we process data from back
     * to front. This is possible if src bit depth is at most half as wide as
     * the 32-bit target buffer. The processor cache should not be a problem. */
    if (source_depth == 8) {
        guint8 *src = (guint8 *) buffer->priv->host_array.data;
        const float scale = normalize ? 255.0f : 1.0f;

        for (gint index = (((gint) num_pixels) - 1); index >= 0; index--)
            dst[index] = ((gfloat) src[index]) / scale;
    }
    else if (source_depth == 16) {
        guint16 *src = (guint16 *) buffer->priv->host_array.data;
        const float scale = normalize ? 65535.0f : 1.0f;

        for (gint index = (((gint) num_pixels) - 1); index >= 0; index--)
            dst[index] = ((gfloat) src[index]) / scale;
    }
}

/**
 * ufo_buffer_fill_with_value:
 * @buffer: A #UfoBuffer.
 * @value: Buffer is filled with this value
 *
 * Fill buffer with the same value.
 */
void ufo_buffer_fill_with_value (UfoBuffer* buffer, gfloat value)
{
    UfoBufferPrivate *priv;
    gfloat *data;
    guint n_elements = 1;

    g_return_if_fail (UFO_IS_BUFFER(buffer));

    priv = UFO_BUFFER_GET_PRIVATE (buffer);
    data = ufo_buffer_get_host_array (buffer, NULL);

    for (guint i = 0; i < priv->host_array.num_dims; i++)
        n_elements *= priv->host_array.dim_size[i];

    for (guint i = 0; i < n_elements; i++)
        data[i] = value;

    priv->location = HOST_ARRAY_VALID;
}

/**
 * ufo_buffer_set_cl_mem:
 * @buffer: A #UfoBuffer.
 * @mem: A cl_mem object.
 *
 * Set OpenCL memory object that is used to up and download data.
 */
void
ufo_buffer_set_cl_mem(UfoBuffer *buffer, gpointer mem)
{
    g_return_if_fail (UFO_IS_BUFFER(buffer) || (mem != NULL));
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE (buffer);
    cl_mem mem_object = (cl_mem) mem;
    gsize mem_size;

    CHECK_OPENCL_ERROR (clGetMemObjectInfo (mem_object, CL_MEM_SIZE, sizeof(gsize), &mem_size, NULL));

    if (mem_size != priv->size)
        g_warning ("Buffer size is different from OpenCL memory object size");

    priv->device_array = (cl_mem) mem_object;
}

/**
 * ufo_buffer_attach_event:
 * @buffer: A #UfoBuffer.
 * @event: A cl_event object.
 *
 * Attach an OpenCL event to a buffer that must be finished before anything else
 * can be done with this buffer.
 */
void
ufo_buffer_attach_event(UfoBuffer *buffer, gpointer event)
{
    g_return_if_fail(UFO_IS_BUFFER(buffer) || (event != NULL));
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
void
ufo_buffer_get_events(UfoBuffer *buffer, gpointer **events, guint *num_events)
{
    g_return_if_fail(UFO_IS_BUFFER(buffer) || (events != NULL) || (num_events != NULL));
    *num_events = buffer->priv->current_event_index;
    *events = (gpointer *) buffer->priv->events;
}

/**
 * ufo_buffer_clear_events:
 * @buffer: A #UfoBuffer.
 *
 * Clear and release events associated with a buffer
 */
void
ufo_buffer_clear_events(UfoBuffer *buffer)
{
    g_return_if_fail(UFO_IS_BUFFER(buffer));
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE(buffer);

    for (guint i = 0; i < priv->current_event_index; i++)
        clReleaseEvent(priv->events[i]);

    priv->current_event_index = 0;
}

/**
 * ufo_buffer_get_transfer_timer: (skip)
 * @buffer: A #UfoBuffer.
 *
 * Each buffer has a timer object that measures time spent for transfering data
 * between host and device.
 *
 * Returns: A #GTimer associated with this buffer
 */
GTimer *
ufo_buffer_get_transfer_timer(UfoBuffer *buffer)
{
    g_return_val_if_fail(UFO_IS_BUFFER(buffer), NULL);
    return buffer->priv->timer;
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
void
ufo_buffer_set_host_array(UfoBuffer *buffer, gfloat *data, gsize num_bytes, GError **error)
{
    g_return_if_fail(UFO_IS_BUFFER(buffer) || (data != NULL));
    UfoBufferPrivate *priv = buffer->priv;

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
        ufo_buffer_alloc_host_mem (buffer);

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
gfloat *
ufo_buffer_get_host_array(UfoBuffer *buffer, gpointer command_queue)
{
    g_return_val_if_fail(UFO_IS_BUFFER(buffer), NULL);
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE(buffer);

    switch (priv->location) {
        case HOST_ARRAY_VALID:
            /* This can be the case, when a buffer's device memory was invalidated
             * but no host memory has been allocated before. */
            if (priv->host_array.data == NULL)
                ufo_buffer_alloc_host_mem (buffer);

            break;

        case DEVICE_ARRAY_VALID:
            if (priv->host_array.data == NULL)
                ufo_buffer_alloc_host_mem (buffer);

            g_timer_start(priv->timer);
            CHECK_OPENCL_ERROR(clEnqueueReadBuffer(command_queue,
                                            priv->device_array,
                                            CL_TRUE,
                                            0, priv->size,
                                            priv->host_array.data,
                                            0, NULL, NULL));
            g_timer_stop(priv->timer);
            /* TODO: Can we release the events here? */
            priv->current_event_index = 0;
            priv->location = HOST_ARRAY_VALID;
            break;

        case NO_DATA:
            ufo_buffer_alloc_host_mem (buffer);
            priv->location = HOST_ARRAY_VALID;
            break;
    }

    return priv->host_array.data;
}

/**
 * ufo_buffer_get_device_array: (skip)
 * @buffer: A #UfoBuffer
 * @command_queue: A cl_command_queue object that is used to access the device
 * memory.
 *
 * Get OpenCL memory object that is used to up and download data.
 *
 * Return value: OpenCL memory object associated with this #UfoBuffer.
 */
gpointer
ufo_buffer_get_device_array(UfoBuffer *buffer, gpointer command_queue)
{
    g_return_val_if_fail(UFO_IS_BUFFER(buffer) || (command_queue != NULL), NULL);
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE(buffer);
    cl_event event;
    cl_int cl_err = CL_SUCCESS;

    switch (priv->location) {
        case HOST_ARRAY_VALID:
            if (priv->device_array == NULL)
                priv->device_array = clCreateBuffer(NULL, CL_MEM_READ_WRITE, priv->size, NULL, &cl_err);

            CHECK_OPENCL_ERROR (cl_err);

            g_timer_start(priv->timer);
            CHECK_OPENCL_ERROR(clEnqueueWriteBuffer((cl_command_queue) command_queue,
                                             priv->device_array,
                                             CL_TRUE,
                                             0, priv->size,
                                             priv->host_array.data,
                                             0, NULL, &event));
            g_timer_stop(priv->timer);
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

/**
 * ufo_buffer_swap_host_arrays:
 * @a: A #UfoBuffer
 * @b: A #UfoBuffer
 *
 * Swap host array pointers of @a and @b and mark host arrays valid.
 */
void
ufo_buffer_swap_host_arrays(UfoBuffer *a, UfoBuffer *b)
{
    g_return_if_fail(UFO_IS_BUFFER(a) && UFO_IS_BUFFER(b));

    UfoBufferPrivate *pa = a->priv;
    UfoBufferPrivate *pb = b->priv;

    /* TODO: also check size of dimensions? */
    g_return_if_fail(pa->host_array.num_dims == pb->host_array.num_dims);
    g_return_if_fail((pa->host_array.data != NULL) || (pb->host_array.data != NULL));

    if (pa->host_array.data == NULL)
        ufo_buffer_alloc_host_mem (a);

    if (pb->host_array.data == NULL)
        ufo_buffer_alloc_host_mem (b);

    gfloat *tmp = pa->host_array.data;
    pa->host_array.data = pb->host_array.data;
    pb->host_array.data = tmp;

    pa->location = HOST_ARRAY_VALID;
    pb->location = HOST_ARRAY_VALID;
}

/**
 * ufo_buffer_param_spec:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #UfoBufferParamSpec instance specifying a #UFO_TYPE_BUFFER
 * property.
 *
 * Returns: (transfer none): a newly created parameter specification
 *
 * @see g_param_spec_internal() for details on property names.
 */
GParamSpec *
ufo_buffer_param_spec(const gchar *name, const gchar *nick, const gchar *blurb, UfoBuffer *default_value, GParamFlags flags)
{
    UfoBufferParamSpec *bspec;

    bspec = g_param_spec_internal(UFO_TYPE_PARAM_BUFFER,
            name, nick, blurb, flags);

    return G_PARAM_SPEC(bspec);
}


static void
ufo_filter_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    UfoBuffer *buffer = UFO_BUFFER(object);

    switch (property_id) {
        case PROP_ID:
            buffer->priv->id = g_value_get_int(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_filter_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    UfoBuffer *buffer = UFO_BUFFER(object);

    switch (property_id) {
        case PROP_ID:
            g_value_set_int(value, buffer->priv->id);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_buffer_finalize(GObject *gobject)
{
    UfoBuffer *buffer = UFO_BUFFER (gobject);
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE (buffer);

    if (priv->host_array.data != NULL)
        g_free (priv->host_array.data);

    if (priv->device_array != NULL)
        CHECK_OPENCL_ERROR (clReleaseMemObject (priv->device_array));

    priv->host_array.data = NULL;
    priv->device_array = NULL;

    g_timer_destroy(priv->timer);
    g_free(priv->events);

    G_OBJECT_CLASS(ufo_buffer_parent_class)->finalize(gobject);
}

static void
ufo_buffer_class_init(UfoBufferClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = ufo_filter_set_property;
    gobject_class->get_property = ufo_filter_get_property;
    gobject_class->finalize = ufo_buffer_finalize;

    buffer_properties[PROP_ID] =
        g_param_spec_int("id",
                         "ID of this buffer",
                         "ID of this buffer",
                         -1, G_MAXINT, 0,
                         G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_ID, buffer_properties[PROP_ID]);

    g_type_class_add_private(klass, sizeof(UfoBufferPrivate));
}

static void
ufo_buffer_init(UfoBuffer *buffer)
{
    UfoBufferPrivate *priv;
    buffer->priv = priv = UFO_BUFFER_GET_PRIVATE(buffer);
    priv->id = -1;
    priv->device_array = NULL;
    priv->host_array.data = NULL;
    priv->host_array.num_dims = 0;
    priv->location = NO_DATA;
    priv->current_event_index = 0;
    priv->num_total_events = 8;
    priv->events = g_malloc0(priv->num_total_events * sizeof(cl_event));
    priv->time_upload = 0;
    priv->time_download = 0;
    priv->timer = g_timer_new();
}

static void
ufo_buffer_param_init(GParamSpec *pspec)
{
    UfoBufferParamSpec *bspec = UFO_BUFFER_PARAM_SPEC(pspec);

    bspec->default_value = NULL;
}

static void
ufo_buffer_param_set_default(GParamSpec *pspec, GValue *value)
{
    UfoBufferParamSpec *bspec = UFO_BUFFER_PARAM_SPEC(pspec);

    bspec->default_value = NULL;
    g_value_unset(value);
}

GType
ufo_buffer_param_get_type()
{
    static GType type = 0;

    if (type == 0) {
        GParamSpecTypeInfo pspec_info = {
            sizeof(UfoBufferParamSpec),     /* instance_size */
            16,                             /* n_preallocs */
            ufo_buffer_param_init,          /* instance_init */
            0,                              /* value_type */
            NULL,                           /* finalize */
            ufo_buffer_param_set_default,   /* value_set_default */
            NULL,                           /* value_validate */
            NULL,                           /* values_cmp */
        };
        pspec_info.value_type = UFO_TYPE_BUFFER;
        type = g_param_type_register_static(g_intern_static_string("UfoBufferParam"), &pspec_info);
        g_assert(type == UFO_TYPE_PARAM_BUFFER);
    }

    return type;
}
