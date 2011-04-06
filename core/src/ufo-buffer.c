#include <string.h>
#include <CL/cl.h>
#include "ufo-buffer.h"

G_DEFINE_TYPE(UfoBuffer, ufo_buffer, G_TYPE_OBJECT);

#define UFO_BUFFER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BUFFER, UfoBufferPrivate))

typedef enum {
    NO_DATA,
    CPU_DATA_VALID,
    GPU_DATA_VALID
} datastates;

struct _UfoBufferPrivate {
    gint32      width;
    gint32      height;

    float       *cpu_data;
    cl_mem      gpu_data;
    datastates  state;
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
UfoBuffer *ufo_buffer_new(gint32 width, gint32 height)
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

void ufo_buffer_set_cpu_data(UfoBuffer *self, float *data)
{
    const gsize num_bytes = self->priv->width * self->priv->height * sizeof(float);
    if (self->priv->cpu_data == NULL)
        self->priv->cpu_data = g_malloc0(num_bytes);

    memcpy(self->priv->cpu_data, data, num_bytes);
    self->priv->state = CPU_DATA_VALID;
}

/*
 * \brief Spread raw data 
 *
 * This method is used to re-interpret the raw data put in with
 * ufo_buffer_set_cpu_data().
 */
void ufo_buffer_reinterpret(UfoBuffer *self, gint source_depth)
{
    float *dst = self->priv->cpu_data;
    /* To save a memory allocation and several copies, we process data from back
     * to front. This is possible if src bit depth is at most half as wide as
     * the 32-bit target buffer. The processor cache should not be a problem. */
    if (source_depth == UFO_BUFFER_DEPTH_8) {
        guint8 *src = (guint8 *) self->priv->cpu_data;
        for (int index = self->priv->width * self->priv->height; index >= 0; index--)
            dst[index] = src[index] / 255.0;
    }
    else if (source_depth == UFO_BUFFER_DEPTH_16) {
        guint16 *src = (guint16 *) self->priv->cpu_data;
        for (int index = self->priv->width * self->priv->height; index >= 0; index--)
            dst[index] = src[index] / 65535.0;
    }
}

/*
 * \brief Get raw pixel data in a flat array (row-column format)
 *
 * \return Pointer to a character array of raw data bytes
 */
float* ufo_buffer_get_cpu_data(UfoBuffer *self)
{
    switch (self->priv->state) {
        case CPU_DATA_VALID:
            break;
        case GPU_DATA_VALID:
            /* TODO: download from gpu */
            self->priv->state = CPU_DATA_VALID;
            break;
        case NO_DATA:
            self->priv->cpu_data = g_malloc0(self->priv->width * self->priv->height * sizeof(float));
            break;
    }

    return self->priv->cpu_data;
}

cl_mem ufo_buffer_get_gpu_data(UfoBuffer *self)
{
    switch (self->priv->state) {
        case CPU_DATA_VALID:
            /* TODO: upload to gpu */
            self->priv->state = GPU_DATA_VALID;
            break;
        case GPU_DATA_VALID:
            break;
        case NO_DATA:
            return NULL;
    }
    return self->priv->gpu_data;
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
    priv->state = NO_DATA;
}
