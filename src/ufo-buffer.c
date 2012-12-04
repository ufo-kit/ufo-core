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

#include <ufo-buffer.h>
#include <ufo-resources.h>

G_DEFINE_TYPE(UfoBuffer, ufo_buffer, G_TYPE_OBJECT)

#define UFO_BUFFER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BUFFER, UfoBufferPrivate))

enum {
    PROP_0,
    PROP_ID,
    PROP_STRUCTURE,
    N_PROPERTIES
};

typedef struct {
    guint num_dims;
    gfloat *data;
    guint dim_size[UFO_BUFFER_MAX_NDIMS];
    guint dim_stride[UFO_BUFFER_MAX_NDIMS];
} nd_array;

struct _UfoBufferPrivate {
    nd_array            host_array;
    cl_mem              device_array;
    cl_context          context;
    cl_command_queue    last_queue;
    gsize               size;   /**< size of buffer in bytes */
    UfoMemLocation      location;
    GTimer             *timer;
};

static void
alloc_mem (UfoBufferPrivate *priv,
           UfoRequisition *requisition)
{
    cl_int err;

    if (priv->host_array.data != NULL)
        g_free (priv->host_array.data);

    if (priv->device_array != NULL)
        clReleaseMemObject (priv->device_array);

    priv->size = sizeof(gfloat);
    priv->host_array.num_dims = requisition->n_dims;

    for (guint i = 0; i < requisition->n_dims; i++) {
        priv->host_array.dim_size[i] = requisition->dims[i];
        priv->size *= requisition->dims[i];
    }

    priv->host_array.data = g_malloc0 (priv->size);
    priv->device_array = clCreateBuffer (priv->context,
                                         CL_MEM_READ_WRITE, /* XXX: we _should_ evaluate USE_HOST_PTR */
                                         priv->size,
                                         NULL,
                                         &err);
    UFO_RESOURCES_CHECK_CLERR (err);
    priv->location = UFO_LOCATION_HOST;
}

/**
 * ufo_buffer_new:
 * @requisition: (in): size requisition
 * @context: (in): cl_context to use for creating the device array
 *
 * Create a new #UfoBuffer.
 *
 * Return value: A new #UfoBuffer with the given dimensions.
 */
UfoBuffer *
ufo_buffer_new (UfoRequisition *requisition,
                gpointer context)
{
    UfoBuffer *buffer;

    g_return_val_if_fail ((requisition->n_dims <= UFO_BUFFER_MAX_NDIMS), NULL);
    buffer = UFO_BUFFER (g_object_new (UFO_TYPE_BUFFER, NULL));
    buffer->priv->context = context;

    alloc_mem (buffer->priv, requisition);
    return buffer;
}

/**
 * ufo_buffer_get_size:
 * @buffer: A #UfoBuffer
 *
 * Get the number of bytes of raw data that is managed by the @buffer.
 *
 * Returns: The size of @buffer's data.
 */
gsize
ufo_buffer_get_size (UfoBuffer *buffer)
{
    g_return_val_if_fail (UFO_IS_BUFFER (buffer), 0);
    return buffer->priv->size;
}

static void
copy_host_to_host (UfoBufferPrivate *src_priv,
                   UfoBufferPrivate *dst_priv)
{
    g_memmove (dst_priv->host_array.data,
               src_priv->host_array.data,
               src_priv->size);
}

static void
copy_device_to_device (UfoBufferPrivate *src_priv,
                       UfoBufferPrivate *dst_priv)
{
    cl_event event;

    clEnqueueCopyBuffer (NULL,
                         src_priv->device_array,
                         dst_priv->device_array,
                         0, 0,                      /* offsets */
                         src_priv->size,
                         0, NULL, &event);

    clWaitForEvents (1, &event);
    clReleaseEvent (event);
}

static void
ufo_buffer_to_host (UfoBuffer *buffer, gpointer cmd_queue)
{
    UfoBufferPrivate *priv;
    cl_int cl_err;
    cl_command_queue queue;

    g_return_if_fail (UFO_IS_BUFFER (buffer));
    priv = buffer->priv;

    if (priv->location == UFO_LOCATION_HOST)
        return;

    queue = cmd_queue == NULL ? priv->last_queue : cmd_queue;
    priv->last_queue = cmd_queue;

    cl_err = clEnqueueReadBuffer (queue,
                                  priv->device_array,
                                  CL_TRUE,
                                  0, priv->size,
                                  priv->host_array.data,
                                  0, NULL, NULL);

    priv->location = UFO_LOCATION_HOST;
    UFO_RESOURCES_CHECK_CLERR (cl_err);
}

static void
ufo_buffer_to_device (UfoBuffer *buffer, gpointer cmd_queue)
{
    UfoBufferPrivate *priv;
    cl_int cl_err;
    cl_command_queue queue;

    g_return_if_fail (UFO_IS_BUFFER (buffer));
    priv = buffer->priv;

    if (priv->location == UFO_LOCATION_DEVICE)
        return;

    queue = cmd_queue == NULL ? priv->last_queue : cmd_queue;
    priv->last_queue = cmd_queue;

    cl_err = clEnqueueWriteBuffer ((cl_command_queue) queue,
                                   priv->device_array,
                                   CL_TRUE,
                                   0, priv->size,
                                   priv->host_array.data,
                                   0, NULL, NULL);

    priv->location = UFO_LOCATION_DEVICE;
    UFO_RESOURCES_CHECK_CLERR (cl_err);
}

/**
 * ufo_buffer_copy:
 * @src: Source #UfoBuffer
 * @dst: Destination #UfoBuffer
 *
 * Copy contents of @src to @dst. The final memory location is determined by the
 * destination buffer.
 */
void
ufo_buffer_copy (UfoBuffer *src, UfoBuffer *dst)
{
    g_return_if_fail (UFO_IS_BUFFER (src) && UFO_IS_BUFFER (dst));
    g_return_if_fail (src->priv->size == dst->priv->size);

    /* Copy depending on location */
    if (src->priv->location == dst->priv->location) {
        switch (src->priv->location) {
            case UFO_LOCATION_HOST:
                copy_host_to_host (src->priv, dst->priv);
                break;
            case UFO_LOCATION_DEVICE:
                copy_device_to_device (src->priv, dst->priv);
                break;
            default:
                g_warning ("oops, we should not copy invalid data");
        }
    }
    else {
        if (dst->priv->location == UFO_LOCATION_HOST) {
            ufo_buffer_to_host (src, NULL);
            copy_host_to_host (src->priv, dst->priv);
        }
        else {
            ufo_buffer_to_device (src, NULL);
            copy_device_to_device (src->priv, src->priv);
        }
    }
}

/**
 * ufo_buffer_dup:
 * @buffer: A #UfoBuffer
 *
 * Create a new buffer with the same requisition as @buffer. Note, that this is
 * not a copy of @buffer!
 *
 * Returns: (transfer full): A #UfoBuffer with the same size as @buffer.
 */
UfoBuffer *
ufo_buffer_dup (UfoBuffer *buffer)
{
    UfoBuffer *copy;
    UfoRequisition requisition;

    ufo_buffer_get_requisition (buffer, &requisition);
    copy = ufo_buffer_new (&requisition, buffer->priv->context);
    return copy;
}

/**
 * ufo_buffer_resize:
 * @buffer: A #UfoBuffer
 * @requisition: A #UfoRequisition structure
 *
 * Resize an existing buffer. If the new requisition has the same size as
 * before, resizing is a no-op.
 *
 * Since: 0.2
 */
void
ufo_buffer_resize (UfoBuffer *buffer,
                   UfoRequisition *requisition)
{
    UfoBufferPrivate *priv;

    g_return_if_fail (UFO_IS_BUFFER (buffer));

    priv = UFO_BUFFER_GET_PRIVATE (buffer);

    if (priv->host_array.data != NULL) {
        g_free (priv->host_array.data);
        priv->host_array.data = NULL;
    }

    if (priv->device_array != NULL) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->device_array));
        priv->device_array = NULL;
    }

    alloc_mem (priv, requisition);
}

/**
 * ufo_buffer_cmp_dimensions:
 * @buffer: A #UfoBuffer
 * @requisition: #UfoRequisition
 *
 * Compare the size of @buffer with a given @requisition.
 *
 * Returns: value < 0, 0 or > 0 if requisition is smaller, equal or larger.
 */
gint
ufo_buffer_cmp_dimensions (UfoBuffer *buffer,
                           UfoRequisition *requisition)
{
    gint result;
    g_return_val_if_fail (UFO_IS_BUFFER(buffer), FALSE);

    result = 0;

    for (guint i = 0; i < buffer->priv->host_array.num_dims; i++) {
        gint req_dim = (gint) requisition->dims[i];
        gint host_dim = (gint) buffer->priv->host_array.dim_size[i];
        result += req_dim - host_dim;
    }

    return result;
}

/**
 * ufo_buffer_get_requisition:
 * @buffer: A #UfoBuffer
 * @requisition: (out): A location to store the requisition of @buffer
 *
 * Return the size of @buffer.
 */
void
ufo_buffer_get_requisition (UfoBuffer *buffer,
                            UfoRequisition *requisition)
{
    UfoBufferPrivate *priv;

    g_return_if_fail (UFO_IS_BUFFER (buffer) && (requisition != NULL));
    priv = buffer->priv;
    requisition->n_dims = priv->host_array.num_dims;

    for (guint i = 0; i < priv->host_array.num_dims; i++)
        requisition->dims[i] = priv->host_array.dim_size[i];
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
ufo_buffer_get_2d_dimensions (UfoBuffer *buffer,
                              guint *width,
                              guint *height)
{
    g_return_if_fail (UFO_IS_BUFFER(buffer) || (width != NULL) || (height != NULL));
    *width = buffer->priv->host_array.dim_size[0];
    *height = buffer->priv->host_array.dim_size[1];
}

/**
 * ufo_buffer_get_host_array:
 * @buffer: A #UfoBuffer.
 * @cmd_queue: (allow-none): A cl_command_queue object or %NULL.
 *
 * Returns a flat C-array containing the raw float data.
 *
 * Returns: Float array.
 */
gfloat *
ufo_buffer_get_host_array (UfoBuffer *buffer, gpointer cmd_queue)
{
    g_return_val_if_fail (UFO_IS_BUFFER (buffer), NULL);
    ufo_buffer_to_host (buffer, cmd_queue);
    return buffer->priv->host_array.data;
}

/**
 * ufo_buffer_get_device_array:
 * @buffer: A #UfoBuffer.
 * @cmd_queue: (allow-none): A cl_command_queue object or %NULL.
 *
 * Return the current cl_mem object of @buffer. If the data is not yet in device
 * memory, it is transfered via @cmd_queue to the object. If @cmd_queue is %NULL
 * @cmd_queue, the last used command queue is used.
 *
 * Returns: (transfer none): A cl_mem object associated with @buffer.
 */
gpointer
ufo_buffer_get_device_array (UfoBuffer *buffer, gpointer cmd_queue)
{
    g_return_val_if_fail (UFO_IS_BUFFER (buffer), NULL);
    ufo_buffer_to_device (buffer, cmd_queue);
    return buffer->priv->device_array;
}

/**
 * ufo_buffer_discard_location:
 * @buffer: A #UfoBuffer
 * @location: Location to discard
 *
 * Discard @location and use "other" location without copying to it first.
 */
void
ufo_buffer_discard_location (UfoBuffer *buffer,
                             UfoMemLocation location)
{
    g_return_if_fail (UFO_IS_BUFFER (buffer));
    buffer->priv->location = location == UFO_LOCATION_HOST ? UFO_LOCATION_DEVICE : UFO_LOCATION_HOST;
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
ufo_buffer_finalize (GObject *gobject)
{
    UfoBuffer *buffer = UFO_BUFFER (gobject);
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE (buffer);

    if (priv->host_array.data != NULL) {
        g_free (priv->host_array.data);
        priv->host_array.data = NULL;
    }

    if (priv->device_array != NULL) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->device_array));
        priv->device_array = NULL;
    }

    if (priv->timer != NULL) {
        g_timer_destroy (priv->timer);
        priv->timer = NULL;
    }

    G_OBJECT_CLASS(ufo_buffer_parent_class)->finalize(gobject);
}

static void
ufo_buffer_class_init (UfoBufferClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = ufo_buffer_finalize;

    g_type_class_add_private(klass, sizeof(UfoBufferPrivate));
}

static void
ufo_buffer_init (UfoBuffer *buffer)
{
    UfoBufferPrivate *priv;
    buffer->priv = priv = UFO_BUFFER_GET_PRIVATE(buffer);
    priv->last_queue = NULL;
    priv->device_array = NULL;
    priv->host_array.data = NULL;
    priv->host_array.num_dims = 0;
    priv->timer = g_timer_new ();
    g_timer_stop (priv->timer);
}

static void
ufo_buffer_param_init (GParamSpec *pspec)
{
    UfoBufferParamSpec *bspec = UFO_BUFFER_PARAM_SPEC(pspec);

    bspec->default_value = NULL;
}

static void
ufo_buffer_param_set_default (GParamSpec *pspec, GValue *value)
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
