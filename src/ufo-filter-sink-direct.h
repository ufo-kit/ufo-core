#ifndef __UFO_FILTER_SINK_DIRECT_H
#define __UFO_FILTER_SINK_DIRECT_H

#include <glib-object.h>

#include "ufo-buffer.h"
#include "ufo-filter-sink.h"

G_BEGIN_DECLS

#define UFO_TYPE_FILTER_SINK_DIRECT             (ufo_filter_sink_direct_get_type())
#define UFO_FILTER_SINK_DIRECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_SINK_DIRECT, UfoFilterSinkDirect))
#define UFO_IS_FILTER_SINK_DIRECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_SINK_DIRECT))
#define UFO_FILTER_SINK_DIRECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_SINK_DIRECT, UfoFilterSinkDirectClass))
#define UFO_IS_FILTER_SINK_DIRECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_SINK_DIRECT))
#define UFO_FILTER_SINK_DIRECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_SINK_DIRECT, UfoFilterSinkDirectClass))

typedef struct _UfoFilterSinkDirect           UfoFilterSinkDirect;
typedef struct _UfoFilterSinkDirectClass      UfoFilterSinkDirectClass;
typedef struct _UfoFilterSinkDirectPrivate    UfoFilterSinkDirectPrivate;

/**
 * UfoFilterSinkDirect:
 *
 * The contents of this object is opaque to the user.
 */
struct _UfoFilterSinkDirect {
    /*< private >*/
    UfoFilterSink parent;

    UfoFilterSinkDirectPrivate *priv;
};

/**
 * UfoFilterSinkDirectClass:
 *
 * @parent: the parent class
 */
struct _UfoFilterSinkDirectClass {
    UfoFilterSinkClass parent;
};

UfoBuffer * ufo_filter_sink_direct_pop      (UfoFilterSinkDirect  *filter);
void        ufo_filter_sink_direct_release  (UfoFilterSinkDirect  *filter,
                                             UfoBuffer            *buffer);
GType       ufo_filter_sink_direct_get_type (void);

G_END_DECLS

#endif
