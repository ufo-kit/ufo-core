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

#define UFO_BUFFER_ERROR ufo_buffer_error_quark()

typedef enum {
    UFO_BUFFER_ERROR_WRONG_SIZE
} UfoBufferError;

typedef enum {
    UFO_LOCATION_INVALID,
    UFO_LOCATION_HOST,
    UFO_LOCATION_DEVICE
} UfoMemLocation;

typedef struct _UfoBuffer           UfoBuffer;
typedef struct _UfoBufferClass      UfoBufferClass;
typedef struct _UfoBufferPrivate    UfoBufferPrivate;
typedef struct _UfoBufferParamSpec  UfoBufferParamSpec;
typedef struct _UfoRequisition      UfoRequisition;

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
#define UFO_BUFFER_MAX_NDIMS 8

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
 * UfoRequisition:
 * @n_dims: Number of dimensions
 * @dims: Size of dimension
 *
 * Used to specify buffer size requirements.
 */
struct _UfoRequisition {
    guint n_dims;
    guint dims[UFO_BUFFER_MAX_NDIMS];
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

UfoBuffer*  ufo_buffer_new                  (UfoRequisition *requisition,
                                             gpointer        context);
void        ufo_buffer_resize               (UfoBuffer      *buffer,
                                             UfoRequisition *requisition);
gint        ufo_buffer_cmp_dimensions       (UfoBuffer      *buffer,
                                             UfoRequisition *requisition);
void        ufo_buffer_get_requisition      (UfoBuffer      *buffer,
                                             UfoRequisition *requisition);
gsize       ufo_buffer_get_size             (UfoBuffer      *buffer);
void        ufo_buffer_get_2d_dimensions    (UfoBuffer      *buffer,
                                             guint          *width,
                                             guint          *height);
void        ufo_buffer_copy                 (UfoBuffer      *src,
                                             UfoBuffer      *dst);
UfoBuffer  *ufo_buffer_dup                  (UfoBuffer      *buffer);
gfloat*     ufo_buffer_get_host_array       (UfoBuffer      *buffer,
                                             gpointer        cmd_queue);
gpointer    ufo_buffer_get_device_array     (UfoBuffer      *buffer,
                                             gpointer        cmd_queue);
void        ufo_buffer_discard_location     (UfoBuffer      *buffer,
                                             UfoMemLocation  location);
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
