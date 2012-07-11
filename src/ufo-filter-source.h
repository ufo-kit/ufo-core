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
 * The contents of this object is opaque to the user.
 */
struct _UfoFilterSource {
    /*< private >*/
    UfoFilter parent;

    UfoFilterSourcePrivate *priv;
};

/**
 * UfoFilterSourceClass:
 * @parent: the parent class
 * @initialize: the @initialize function is called by an UfoBaseScheduler to set
 *      up a filter before actual execution happens.
 * @generate: the @generate function produces data for subsequent filters. It
 *      returns TRUE if is produced.
 */
struct _UfoFilterSourceClass {
    UfoFilterClass parent;

    /* overridable */
    void     (*initialize)  (UfoFilterSource    *filter,
                             guint             **output_dim_sizes,
                             GError            **error);
    gboolean (*generate)    (UfoFilterSource    *filter,
                             UfoBuffer          *output[],
                             gpointer            cmd_queue,
                             GError            **error);
};

void     ufo_filter_source_initialize (UfoFilterSource   *filter,
                                       guint            **output_dim_sizes,
                                       GError           **error);
gboolean ufo_filter_source_generate   (UfoFilterSource   *filter,
                                       UfoBuffer         *output[],
                                       gpointer           cmd_queue,
                                       GError            **error);
GType    ufo_filter_source_get_type   (void);

G_END_DECLS

#endif
