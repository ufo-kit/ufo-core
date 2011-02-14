#ifndef __UFO_BUFFER_H
#define __UFO_BUFFER_H

#include <glib.h>
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

struct _UfoBuffer {
    GObject parent_instance;

    /* public */
    gboolean shared;

    /* private */
    UfoBufferPrivate *priv;
};

struct _UfoBufferClass {
    GObjectClass parent_class;

    /* class members */

    /* virtual public methods */
    gboolean (*malloc) (UfoBuffer *self);
};

/* virtual public methods */
gboolean ufo_buffer_malloc(UfoBuffer *self);

/* non-virtual public methods */
void ufo_buffer_set_dimensions(UfoBuffer *self, gint32 width, gint32 height);
void ufo_buffer_get_dimensions(UfoBuffer *self, gint32 *width, gint32 *height);
void ufo_buffer_set_bytes_per_pixel(UfoBuffer *self, gint32 bytes_per_pixel);
gint32 ufo_buffer_get_bytes_per_pixel(UfoBuffer *self);
gchar* ufo_buffer_get_raw_bytes(UfoBuffer *self);

GType ufo_buffer_get_type(void);

G_END_DECLS

#endif
