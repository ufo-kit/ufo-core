#include "ufo-buffer.h"

G_DEFINE_TYPE(UfoBuffer, ufo_buffer, G_TYPE_OBJECT);

#define UFO_BUFFER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BUFFER, UfoBufferPrivate))

struct _UfoBufferPrivate {
    gint32      width;
    gint32      height;

    float       *cpu_data;
    gboolean    is_cpu_data_valid;

    float       *gpu_data;
    gboolean    is_gpu_data_valid;
};


/* 
 * non-virtual public methods 
 */

/*
 * \brief Create a new buffer with given dimensions
 *
 * \param[in] width Width of the two-dimensional buffer
 * \param[in] height Height of the two-dimensional buffer
 * \param[in] bytes_per_pixel Number of bytes per pixel
 *
 * \return Buffer with allocated memory
 */
UfoBuffer *ufo_buffer_new(gint32 width, gint32 height, gint32 bytes_per_pixel)
{
    UfoBuffer *buffer = g_object_new(UFO_TYPE_BUFFER, NULL);
    ufo_buffer_set_dimensions(buffer, width, height);
    return buffer;
}

/*
 * \brief Set dimension of buffer data in pixels
 *
 * \param[in] width Width of the two-dimensional buffer
 * \param[in] height Height of the two-dimensional buffer
 */
void ufo_buffer_set_dimensions(UfoBuffer *self, gint32 width, gint32 height)
{
    g_return_if_fail(UFO_IS_BUFFER(self));
    /* FIXME: What to do when buffer already allocated memory? Re-size? */
    self->priv->width = width;
    self->priv->height = height;
}

void ufo_buffer_get_dimensions(UfoBuffer *self, gint32 *width, gint32 *height)
{
    g_return_if_fail(UFO_IS_BUFFER(self));
    *width = self->priv->width;
    *height = self->priv->height;
}

/*
 * \brief Get raw pixel data in a flat array (row-column format)
 *
 * \return Pointer to a character array of raw data bytes
 */
float* ufo_buffer_get_cpu_data(UfoBuffer *self)
{
    if (self->priv->is_cpu_data_valid)
        return self->priv->cpu_data;
    else
        ; /* TODO: download from gpu */
    return NULL;
}

void ufo_buffer_get_gpu_data(UfoBuffer *self)
{
    if (self->priv->is_gpu_data_valid)
        ; /* TODO: return cl_mem handle */
    else
        ; /* TODO: upload data and return cl_mem handle */
}

/* 
 * virtual methods 
 */

static void ufo_buffer_finalize(GObject *gobject)
{
    UfoBuffer *self = UFO_BUFFER(gobject);
    if (self->priv->cpu_data)
        g_free(self->priv->cpu_data);

    G_OBJECT_CLASS(ufo_buffer_parent_class)->finalize(gobject);
}

static void ufo_buffer_class_init(UfoBufferClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = ufo_buffer_finalize;

    g_type_class_add_private(klass, sizeof(UfoBufferPrivate));
}

static void ufo_buffer_init(UfoBuffer *self)
{
    /* init public fields */
    self->shared = FALSE;

    /* init private fields */
    UfoBufferPrivate *priv;
    self->priv = priv = UFO_BUFFER_GET_PRIVATE(self);
    priv->width = -1;
    priv->height = -1;
    priv->cpu_data = NULL;
    priv->is_cpu_data_valid = FALSE;
    priv->is_gpu_data_valid = FALSE;
}
