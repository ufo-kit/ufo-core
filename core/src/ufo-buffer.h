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
 * \class UfoBuffer
 * \brief Abstract data container
 *
 * Abstract data container that holds either valid GPU data or CPU data.
 *
 * <b>Signals</b>
 *
 * <b>Properties</b>
 *
 *  - <tt>"finished" : gboolean</tt> — If TRUE, buffer does not contain any
 *      useful data but specifies that no more data is following.
 */
struct _UfoBuffer {
    GObject parent_instance;

    /* private */
    UfoBufferPrivate *priv;
};

struct _UfoBufferClass {
    GObjectClass parent_class;
};

typedef enum {
    UFO_BUFFER_READABLE  = 1 << 0,
    UFO_BUFFER_WRITEABLE = 1 << 1,
    UFO_BUFFER_READWRITE = (UFO_BUFFER_READABLE | UFO_BUFFER_WRITEABLE)
} UfoAccess;

typedef enum {
    UFO_BUFFER_1D,
    UFO_BUFFER_2D,
    UFO_BUFFER_3D,
    UFO_BUFFER_4D
} UfoStructure;

typedef enum {
    UFO_BUFFER_SPACE,
    UFO_BUFFER_FREQUENCY /**< implies interleaved complex numbers */
} UfoDomain;

UfoBuffer *ufo_buffer_new(UfoStructure structure, const gint32 dimensions[4]);
UfoBuffer *ufo_buffer_copy(UfoBuffer *buffer, gpointer command_queue);

void ufo_buffer_increment_id(UfoBuffer *buffer);

gsize ufo_buffer_get_size(UfoBuffer *buffer);
gint ufo_buffer_get_id(UfoBuffer *buffer);
void ufo_buffer_get_dimensions(UfoBuffer *buffer, gint32 *dimensions);
void ufo_buffer_get_2d_dimensions(UfoBuffer *buffer, gint32 *width, gint32 *height);

void ufo_buffer_reinterpret(UfoBuffer *self, gsize source_depth, gsize n);
void ufo_buffer_set_cpu_data(UfoBuffer *self, float *data, gsize n, GError **error);
float* ufo_buffer_get_cpu_data(UfoBuffer *self, gpointer command_queue);
gpointer ufo_buffer_get_gpu_data(UfoBuffer *self, gpointer command_queue);
void ufo_buffer_invalidate_gpu_data(UfoBuffer *self);
void ufo_buffer_set_cl_mem(UfoBuffer *self, gpointer mem);
void* ufo_buffer_get_cl_mem(UfoBuffer *buffer);
void ufo_buffer_wait_on_event(UfoBuffer *self, gpointer event);
void ufo_buffer_get_transfer_statistics(UfoBuffer *self, gint *uploads, gint *downloads);
void ufo_buffer_get_transfer_time(UfoBuffer *self, gulong *upload_time, gulong *download_time);

void ufo_buffer_transfer_id(UfoBuffer *from, UfoBuffer *to);

gboolean ufo_buffer_is_finished(UfoBuffer *self);

GType ufo_buffer_get_type(void);

G_END_DECLS

#endif
