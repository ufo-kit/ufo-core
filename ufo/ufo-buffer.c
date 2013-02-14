/*
 * Copyright (C) 2011-2013 Karlsruhe Institute of Technology
 *
 * This file is part of Ufo.
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <ufo/ufo-buffer.h>
#include <ufo/ufo-resources.h>

/**
 * SECTION:ufo-buffer
 * @Short_description: Represents n-dimensional data
 * @Title: UfoBuffer
 */

G_DEFINE_TYPE(UfoBuffer, ufo_buffer, G_TYPE_OBJECT)

#define UFO_BUFFER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BUFFER, UfoBufferPrivate))

enum {
    PROP_0,
    PROP_ID,
    PROP_STRUCTURE,
    N_PROPERTIES
};

struct _UfoBufferPrivate {
    UfoRequisition      requisition;
    gfloat             *host_array;
    cl_mem              device_array;
    cl_context          context;
    cl_command_queue    last_queue;
    gsize               size;   /**< size of buffer in bytes */
    UfoMemLocation      location;
};

static void
copy_requisition (UfoRequisition *src,
                  UfoRequisition *dst)
{
    const guint n_dims = src->n_dims;

    dst->n_dims = n_dims;

    for (guint i = 0; i < n_dims; i++)
        dst->dims[i] = src->dims[i];
}

static gsize
compute_required_size (UfoRequisition *requisition)
{
    gsize size = sizeof (gfloat);

    for (guint i = 0; i < requisition->n_dims; i++)
        size *= requisition->dims[i];

    return size;
}

static void
alloc_host_mem (UfoBufferPrivate *priv)
{
    if (priv->host_array != NULL)
        g_free (priv->host_array);

    priv->host_array = g_malloc0 (priv->size);
}

static void
alloc_device_mem (UfoBufferPrivate *priv)
{
    cl_int err;

    if (priv->device_array != NULL)
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->device_array));

    priv->device_array = clCreateBuffer (priv->context,
                                         CL_MEM_READ_WRITE, /* XXX: we _should_ evaluate USE_HOST_PTR */
                                         priv->size,
                                         NULL,
                                         &err);

    UFO_RESOURCES_CHECK_CLERR (err);
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
    UfoBufferPrivate *priv;

    g_return_val_if_fail ((requisition->n_dims <= UFO_BUFFER_MAX_NDIMS), NULL);
    buffer = UFO_BUFFER (g_object_new (UFO_TYPE_BUFFER, NULL));
    priv = buffer->priv;
    priv->context = context;

    priv->size = compute_required_size (requisition);
    copy_requisition (requisition, &priv->requisition);

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
    g_memmove (dst_priv->host_array,
               src_priv->host_array,
               src_priv->size);
}

static void
copy_device_to_device (UfoBufferPrivate *src_priv,
                       UfoBufferPrivate *dst_priv)
{
    cl_event event;
    cl_int errcode;
    cl_command_queue cmd_queue;

    cmd_queue = src_priv->last_queue != NULL ? src_priv->last_queue : dst_priv->last_queue;
    g_assert (cmd_queue != NULL);

    errcode = clEnqueueCopyBuffer (cmd_queue,
                                   src_priv->device_array,
                                   dst_priv->device_array,
                                   0, 0,                      /* offsets */
                                   src_priv->size,
                                   0, NULL, &event);

    UFO_RESOURCES_CHECK_CLERR (errcode);
    UFO_RESOURCES_CHECK_CLERR (clWaitForEvents (1, &event));
    UFO_RESOURCES_CHECK_CLERR (clReleaseEvent (event));
}

static void
transfer_to_host (UfoBufferPrivate *priv,
                  gpointer cmd_queue)
{
    cl_int cl_err;
    cl_command_queue queue;

    queue = cmd_queue == NULL ? priv->last_queue : cmd_queue;
    priv->last_queue = cmd_queue;

    if (priv->location == UFO_LOCATION_HOST)
        return;

    cl_err = clEnqueueReadBuffer (queue,
                                  priv->device_array,
                                  CL_TRUE,
                                  0, priv->size,
                                  priv->host_array,
                                  0, NULL, NULL);

    priv->location = UFO_LOCATION_HOST;
    UFO_RESOURCES_CHECK_CLERR (cl_err);
}

static void
transfer_to_device (UfoBufferPrivate *priv,
                    gpointer cmd_queue)
{
    cl_int cl_err;
    cl_command_queue queue;

    queue = cmd_queue == NULL ? priv->last_queue : cmd_queue;
    priv->last_queue = cmd_queue;
    g_assert (cmd_queue);

    if (priv->location == UFO_LOCATION_DEVICE)
        return;

    cl_err = clEnqueueWriteBuffer ((cl_command_queue) queue,
                                   priv->device_array,
                                   CL_TRUE,
                                   0, priv->size,
                                   priv->host_array,
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
    UfoBufferPrivate *spriv;
    UfoBufferPrivate *dpriv;

    g_return_if_fail (UFO_IS_BUFFER (src) && UFO_IS_BUFFER (dst));
    g_return_if_fail (src->priv->size == dst->priv->size);

    spriv = src->priv;
    dpriv = dst->priv;

    if (spriv->location == dpriv->location) {
        switch (spriv->location) {
            case UFO_LOCATION_HOST:
                copy_host_to_host (spriv, dpriv);
                break;
            case UFO_LOCATION_DEVICE:
                copy_device_to_device (spriv, dpriv);
                break;
            default:
                g_warning ("oops, we should not copy invalid data");
        }
    }
    else {
        cl_command_queue cmd_queue;

        cmd_queue = spriv->last_queue != NULL ? spriv->last_queue : dpriv->last_queue;

        if (cmd_queue == NULL || dpriv->location == UFO_LOCATION_HOST) {
            if (spriv->host_array == NULL)
                alloc_host_mem (spriv);

            transfer_to_host (spriv, cmd_queue);

            if (dpriv->host_array == NULL)
                alloc_host_mem (dpriv);

            copy_host_to_host (spriv, dpriv);
        }
        else {
            if (spriv->device_array == NULL)
                alloc_device_mem (spriv);

            transfer_to_device (spriv, cmd_queue);

            if (dpriv->device_array == NULL)
                alloc_device_mem (dpriv);

            copy_device_to_device (spriv, dpriv);
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

    if (priv->host_array != NULL) {
        g_free (priv->host_array);
        priv->host_array = NULL;
    }

    if (priv->device_array != NULL) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->device_array));
        priv->device_array = NULL;
    }

    copy_requisition (requisition, &priv->requisition);
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
    UfoBufferPrivate *priv;
    gint result;

    g_return_val_if_fail (UFO_IS_BUFFER(buffer), FALSE);

    priv = buffer->priv;
    result = 0;

    for (guint i = 0; i < priv->requisition.n_dims; i++) {
        gint req_dim = (gint) requisition->dims[i];
        gint host_dim = (gint) priv->requisition.dims[i];
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

    copy_requisition (&priv->requisition, requisition);
}

static void
update_last_queue (UfoBufferPrivate *priv,
                   cl_command_queue queue)
{
    if (queue != NULL)
        priv->last_queue = queue;
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
    UfoBufferPrivate *priv;

    g_return_val_if_fail (UFO_IS_BUFFER (buffer), NULL);
    priv = buffer->priv;

    update_last_queue (priv, cmd_queue);

    if (priv->host_array == NULL)
        alloc_host_mem (priv);

    if (priv->device_array != NULL)
        transfer_to_host (priv, cmd_queue);

    priv->location = UFO_LOCATION_HOST;
    return priv->host_array;
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
    UfoBufferPrivate *priv;

    g_return_val_if_fail (UFO_IS_BUFFER (buffer), NULL);
    priv = buffer->priv;

    update_last_queue (priv, cmd_queue);

    if (priv->device_array == NULL)
        alloc_device_mem (priv);

    if (priv->host_array != NULL)
        transfer_to_device (priv, cmd_queue);

    priv->location = UFO_LOCATION_DEVICE;
    return priv->device_array;
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
 * ufo_buffer_convert:
 * @buffer: A #UfoBuffer
 * @depth: Source bit depth of host data
 *
 * Convert host data according to its @depth to the internal 32-bit floating
 * point representation.
 */
void
ufo_buffer_convert (UfoBuffer *buffer,
                    UfoBufferDepth depth)
{
    UfoBufferPrivate *priv;
    gint n_pixels;
    gfloat *dst;

    g_return_if_fail (UFO_IS_BUFFER (buffer));
    priv = buffer->priv;
    n_pixels = (gint) (priv->size / 4);
    dst = priv->host_array;

    /* To save a memory allocation and several copies, we process data from back
     * to front. This is possible if src bit depth is at most half as wide as
     * the 32-bit target buffer. The processor cache should not be a
     * problem. */
    if (depth == UFO_BUFFER_DEPTH_8U) {
        guint8 *src = (guint8 *) priv->host_array;

        for (gint i = (n_pixels - 1); i >= 0; i--)
            dst[i] = ((gfloat) src[i]);
    }
    else if (depth == UFO_BUFFER_DEPTH_16U) {
        guint16 *src = (guint16 *) priv->host_array;

        for (gint i = (n_pixels - 1); i >= 0; i--)
            dst[i] = ((gfloat) src[i]);
    }
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

    g_free (priv->host_array);
    priv->host_array = NULL;

    if (priv->device_array != NULL) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->device_array));
        priv->device_array = NULL;
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
    priv->host_array = NULL;
    priv->requisition.n_dims = 0;
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
