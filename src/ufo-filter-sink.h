#ifndef __UFO_FILTER_SINK_H
#define __UFO_FILTER_SINK_H

#include <glib-object.h>

#include "ufo-buffer.h"
#include "ufo-filter.h"

G_BEGIN_DECLS

#define UFO_TYPE_FILTER_SINK             (ufo_filter_sink_get_type())
#define UFO_FILTER_SINK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_SINK, UfoFilterSink))
#define UFO_IS_FILTER_SINK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_SINK))
#define UFO_FILTER_SINK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_SINK, UfoFilterSinkClass))
#define UFO_IS_FILTER_SINK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_SINK))
#define UFO_FILTER_SINK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_SINK, UfoFilterSinkClass))

typedef struct _UfoFilterSink           UfoFilterSink;
typedef struct _UfoFilterSinkClass      UfoFilterSinkClass;
typedef struct _UfoFilterSinkPrivate    UfoFilterSinkPrivate;

/**
 * UfoFilterSink:
 *
 * The contents of this object is opaque to the user.
 */
struct _UfoFilterSink {
    /*< private >*/
    UfoFilter parent;
};

/**
 * UfoFilterSinkClass:
 * @parent: the parent class
 * @initialize: the @initialize function is called by an UfoBaseScheduler to set
 *      up a filter before actual execution happens. It receives the first input
 *      data to which the filter can get adjust.
 * @consume: the @consume function implements what is going to happen with the
 *      input.
 */
struct _UfoFilterSinkClass {
    UfoFilterClass parent;

    /* overridable methods */
    void (*initialize)  (UfoFilterSink  *filter,
                         UfoBuffer      *input[],
                         GError        **error);
    void (*consume)     (UfoFilterSink  *filter,
                         UfoBuffer      *input[],
                         gpointer        cmd_queue,
                         GError        **error);
};

void  ufo_filter_sink_initialize (UfoFilterSink  *filter,
                                  UfoBuffer      *input[],
                                  GError        **error);
void  ufo_filter_sink_consume    (UfoFilterSink  *filter,
                                  UfoBuffer      *input[],
                                  gpointer        cmd_queue,
                                  GError        **error);
GType ufo_filter_sink_get_type   (void);

G_END_DECLS

#endif
