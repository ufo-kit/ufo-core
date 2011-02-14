#include "ufo-buffer.h"
#include "ufo-filter.h"

G_DEFINE_TYPE(UfoBuffer, ufo_buffer, G_TYPE_OBJECT);

#define UFO_BUFFER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BUFFER, UfoBufferPrivate))

struct _UfoBufferPrivate {
    UfoFilter *from;
    UfoFilter *to;

    gint32  width;
    gint32  height;
    gssize  bytes_per_pixel;
    gchar   *data;
};


/* 
 * non-virtual public methods 
 */

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
 * \brief Set number of bytes per pixel
 *
 * \param[in] bytes_per_pixel Bytes per pixel of raw data
 */
void ufo_buffer_set_bytes_per_pixel(UfoBuffer *self, gint32 bytes_per_pixel)
{
    g_return_if_fail(UFO_IS_BUFFER(self));
    self->priv->bytes_per_pixel = bytes_per_pixel;
}

/*
 * \brief Get number of bytes per pixel
 *
 * \return Bytes per pixel of raw data
 */
gint32 ufo_buffer_get_bytes_per_pixel(UfoBuffer *self)
{
    return self->priv->bytes_per_pixel;
}

/*
 * \brief Get raw pixel data in a flat array (row-column format)
 *
 * \return Pointer to a character array of raw data bytes
 */
gchar* ufo_buffer_get_raw_bytes(UfoBuffer *self)
{
    return self->priv->data;
}

/*
 * \brief Allocate memory for the specified dimensions and bytes per pixel
 *
 * \return TRUE if allocation was successfull else FALSE
 */
gboolean ufo_buffer_malloc(UfoBuffer *self)
{
   return(UFO_BUFFER_GET_CLASS(self)->malloc(self));
}


/* 
 * virtual methods 
 */

static gboolean ufo_buffer_malloc_default(UfoBuffer *self)
{
    UfoBufferPrivate *priv = self->priv;
    if ((priv->width == -1) || (priv->height == -1) || (priv->bytes_per_pixel == -1))
        return FALSE;

    priv->data = g_malloc0(priv->width * priv->height * priv->bytes_per_pixel);
    return TRUE;
}

static void ufo_buffer_dispose(GObject *gobject)
{
    UfoBuffer *self = UFO_BUFFER(gobject);
    
    if (self->priv->from) {
        g_object_unref(self->priv->from);
        self->priv->from = NULL;
    }

    if (self->priv->to) {
        g_object_unref(self->priv->to);
        self->priv->to = NULL;
    }

    G_OBJECT_CLASS(ufo_buffer_parent_class)->dispose(gobject);
}

static void ufo_buffer_finalize(GObject *gobject)
{
    UfoBuffer *self = UFO_BUFFER(gobject);
    g_free(self->priv->data);

    G_OBJECT_CLASS(ufo_buffer_parent_class)->finalize(gobject);
}

static void ufo_buffer_class_init(UfoBufferClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose = ufo_buffer_dispose;
    gobject_class->finalize = ufo_buffer_finalize;

    g_type_class_add_private(klass, sizeof(UfoBufferPrivate));
    klass->malloc = ufo_buffer_malloc_default;
}

static void ufo_buffer_init(UfoBuffer *self)
{
    /* init public fields */
    self->shared = FALSE;

    /* init private fields */
    UfoBufferPrivate *priv;
    self->priv = priv = UFO_BUFFER_GET_PRIVATE(self);
    priv->from = NULL;
    priv->to = NULL;
    priv->width = -1;
    priv->height = -1;
    priv->bytes_per_pixel = -1;
    priv->data = NULL;
}
