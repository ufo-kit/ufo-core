#ifndef __UFO_FILTER_SOURCE_H
#define __UFO_FILTER_SOURCE_H

#include <glib-object.h>

#include "ufo-buffer.h"
#include "ufo-filter.h"

G_BEGIN_DECLS

#define UFO_TYPE_FILTER_SOURCE             (ufo_filter_source_get_type())
#define UFO_FILTER_SOURCE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_SOURCE, UfoFilterSource))
#define UFO_IS_FILTER_SOURCE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_SOURCE))
#define UFO_FILTER_SOURCE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_SOURCE, UfoFilterSourceClass))
#define UFO_IS_FILTER_SOURCE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_SOURCE))
#define UFO_FILTER_SOURCE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_SOURCE, UfoFilterSourceClass))

typedef struct _UfoFilterSource           UfoFilterSource;
typedef struct _UfoFilterSourceClass      UfoFilterSourceClass;
typedef struct _UfoFilterSourcePrivate    UfoFilterSourcePrivate;

/**
 * UfoFilterSource:
 *
 * Creates #UfoFilterSource instances by loading corresponding shared objects. The
 * contents of the #UfoFilterSource structure are private and should only be
 * accessed via the provided API.
 */
struct _UfoFilterSource {
    /*< private >*/
    UfoFilter parent;

    UfoFilterSourcePrivate *priv;
};

/**
 * UfoFilterSourceClass:
 *
 * #UfoFilterSource class
 */
struct _UfoFilterSourceClass {
    /*< private >*/
    UfoFilterClass parent;

    GError * (*source_initialize) (UfoFilterSource *filter, guint **output_dim_sizes);
    gboolean (*generate) (UfoFilterSource *filter, UfoBuffer *results[], gpointer cmd_queue, GError **error);
};

GError  *ufo_filter_source_initialize (UfoFilterSource   *filter,
                                       guint            **output_dim_sizes);
gboolean ufo_filter_source_generate   (UfoFilterSource   *filter,
                                       UfoBuffer         *results[],
                                       gpointer           cmd_queue,
                                       GError            **error);
GType    ufo_filter_source_get_type   (void);

G_END_DECLS

#endif
