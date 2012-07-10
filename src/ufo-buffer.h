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

#define UFO_TYPE_PARAM_BUFFER       (ufo_buffer_param_get_type())
#define UFO_IS_PARAM_SPEC_BUFFER(pspec)  (G_TYPE_CHECK_INSTANCE_TYPE((pspec), UFO_TYPE_PARAM_BUFFER))
#define UFO_BUFFER_PARAM_SPEC(pspec)     (G_TYPE_CHECK_INSTANCE_CAST((pspec), UFO_TYPE_PARAM_BUFFER, UfoBufferParamSpec))

typedef struct _UfoBuffer           UfoBuffer;
typedef struct _UfoBufferClass      UfoBufferClass;
typedef struct _UfoBufferPrivate    UfoBufferPrivate;
typedef struct _UfoBufferParamSpec  UfoBufferParamSpec;

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
 * UFO_BUFFER_MAX_NDIMS:
 *
 * Maximum number of allowed dimensions. This is a pre-processor macro instead
 * of const variable because of <ulink
 * url="http://c-faq.com/ansi/constasconst.html">C constraints</ulink>.
 */
#define UFO_BUFFER_MAX_NDIMS 32

/**
 * UfoBufferClass:
 *
 * #UfoBuffer class
 */
struct _UfoBufferClass {
    /*< private >*/
    GObjectClass parent_class;
};

/**
 * UfoBufferParamSpec:
 *
 * UfoBufferParamSpec class
 */
struct _UfoBufferParamSpec {
    /*< private >*/
    GParamSpec  parent_instance;

    UfoBuffer   *default_value;
};

UfoBuffer*  ufo_buffer_new                  (guint          num_dims, 
                                             const guint   *dim_size);
void        ufo_buffer_resize               (UfoBuffer     *buffer,
                                             guint          num_dims,
                                             const guint   *dim_size);
void        ufo_buffer_copy                 (UfoBuffer     *from, 
                                             UfoBuffer     *to, 
                                             gpointer       command_queue);
void        ufo_buffer_transfer_id          (UfoBuffer     *from, 
                                             UfoBuffer     *to);
gsize       ufo_buffer_get_size             (UfoBuffer     *buffer);
gint        ufo_buffer_get_id               (UfoBuffer     *buffer);
void        ufo_buffer_get_dimensions       (UfoBuffer     *buffer, 
                                             guint*         num_dims, 
                                             guint**        dim_size);
void        ufo_buffer_get_2d_dimensions    (UfoBuffer*     buffer, 
                                             guint*         width, 
                                             guint*         height);
void        ufo_buffer_reinterpret          (UfoBuffer*     buffer, 
                                             gsize          source_depth, 
                                             gsize          num_pixels, 
                                             gboolean       normalize);
void        ufo_buffer_fill_with_value      (UfoBuffer*     buffer,
                                             gfloat         value);
void        ufo_buffer_set_host_array       (UfoBuffer*     buffer, 
                                             gfloat*        data, 
                                             gsize          num_bytes, 
                                             GError**       error);
gfloat*     ufo_buffer_get_host_array       (UfoBuffer     *buffer, 
                                             gpointer       command_queue);
GTimer*     ufo_buffer_get_transfer_timer   (UfoBuffer     *buffer);
void        ufo_buffer_swap_host_arrays     (UfoBuffer     *a, 
                                             UfoBuffer     *b);
gpointer    ufo_buffer_get_device_array     (UfoBuffer     *buffer, 
                                             gpointer       command_queue);
void        ufo_buffer_invalidate_gpu_data  (UfoBuffer*     buffer);
void        ufo_buffer_set_cl_mem           (UfoBuffer*     buffer, 
                                             gpointer       mem);
void        ufo_buffer_attach_event         (UfoBuffer*     buffer,
                                             gpointer       event);
void        ufo_buffer_get_events           (UfoBuffer*     buffer,
                                             gpointer**     events, 
                                             guint*         num_events);
void        ufo_buffer_clear_events         (UfoBuffer*     buffer);
GType       ufo_buffer_get_type             (void);
GQuark      ufo_buffer_error_quark          (void);

GParamSpec* ufo_buffer_param_spec           (const gchar*   name, 
                                             const gchar*   nick,
                                             const gchar*   blurb, 
                                             UfoBuffer*     default_value, 
                                             GParamFlags    flags);
GType       ufo_buffer_param_get_type       (void);

G_END_DECLS

#endif
