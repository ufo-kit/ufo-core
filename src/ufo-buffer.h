#ifndef __UFO_BUFFER_H
#define __UFO_BUFFER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_BUFFER             (ufo_buffer_get_type())
#define UFO_BUFFER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_BUFFER, UfoBuffer))
#define UFO_IS_BUFFER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_BUFFER))
#define UFO_BUFFER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_BUFFER, UfoBufferClass))
#define UFO_IS_BUFFER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_BUFFER))
#define UFO_BUFFER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_BUFFER, UfoBufferClass))

typedef struct _UfoBuffer           UfoBuffer;
typedef struct _UfoBufferClass      UfoBufferClass;
typedef struct _UfoBufferPrivate    UfoBufferPrivate;

/**
 * UfoBuffer:
 *
 * Represents n-dimensional data. The contents of the #UfoBuffer structure are
 * private and should only be accessed via the provided API.
 */
struct _UfoBuffer {
    /*< private >*/
    GObject parent_instance;

    UfoBufferPrivate *priv;
};

/**
 * UfoBufferClass:
 *
 * #UfoBuffer class
 */
struct _UfoBufferClass {
    /*< private >*/
    GObjectClass parent_class;
};

typedef enum {
    UFO_BUFFER_READABLE  = 1 << 0,
    UFO_BUFFER_WRITEABLE = 1 << 1,
    UFO_BUFFER_READWRITE = (UFO_BUFFER_READABLE | UFO_BUFFER_WRITEABLE)
} UfoAccess;

typedef enum {
    UFO_BUFFER_SPACE,
    UFO_BUFFER_FREQUENCY, /**< implies interleaved complex numbers */
    UFO_BUFFER_HISTOGRAM
} UfoDomain;

UfoBuffer *ufo_buffer_new(int num_dims, const int *dim_size);
void ufo_buffer_set_dimensions(UfoBuffer *buffer, int num_dims, const int *dim_size);

void ufo_buffer_copy(UfoBuffer *from, UfoBuffer *to, gpointer command_queue);

void ufo_buffer_transfer_id(UfoBuffer *from, UfoBuffer *to);

gsize ufo_buffer_get_size(UfoBuffer *buffer);
gint ufo_buffer_get_id(UfoBuffer *buffer);
void ufo_buffer_get_dimensions(UfoBuffer *buffer, int *num_dims, int **dim_size);
void ufo_buffer_get_2d_dimensions(UfoBuffer *buffer, gint32 *width, gint32 *height);

void ufo_buffer_reinterpret(UfoBuffer *buffer, gsize source_depth, gsize num_pixels);
void ufo_buffer_set_host_array(UfoBuffer *buffer, float *data, gsize num_bytes, GError **error);
float* ufo_buffer_get_host_array(UfoBuffer *buffer, gpointer command_queue);

/* gpointer ufo_buffer_get_gpu_data(UfoBuffer *buffer, gpointer command_queue); */
gpointer ufo_buffer_get_device_array(UfoBuffer *buffer, gpointer command_queue);
void ufo_buffer_invalidate_gpu_data(UfoBuffer *buffer);
void ufo_buffer_set_cl_mem(UfoBuffer *buffer, gpointer mem);
gpointer ufo_buffer_get_cl_mem(UfoBuffer *buffer);
void ufo_buffer_get_transfer_time(UfoBuffer *buffer, gulong *upload_time, gulong *download_time);

void ufo_buffer_attach_event(UfoBuffer *buffer, gpointer event);
void ufo_buffer_get_events(UfoBuffer *buffer, gpointer **events, guint *num_events);
void ufo_buffer_clear_events(UfoBuffer *buffer);

GType ufo_buffer_get_type(void);

G_END_DECLS

#endif
