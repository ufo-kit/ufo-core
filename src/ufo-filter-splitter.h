#ifndef __UFO_FILTER_SPLITTER_H
#define __UFO_FILTER_SPLITTER_H

#include <glib-object.h>

#include "ufo-filter.h"

G_BEGIN_DECLS

#define UFO_TYPE_FILTER_SPLITTER             (ufo_filter_splitter_get_type())
#define UFO_FILTER_SPLITTER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_SPLITTER, UfoFilterSplitter))
#define UFO_IS_FILTER_SPLITTER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_SPLITTER))
#define UFO_FILTER_SPLITTER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_SPLITTER, UfoFilterSplitterClass))
#define UFO_IS_FILTER_SPLITTER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_SPLITTER))
#define UFO_FILTER_SPLITTER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_SPLITTER, UfoFilterSplitterClass))

typedef struct _UfoFilterSplitter           UfoFilterSplitter;
typedef struct _UfoFilterSplitterClass      UfoFilterSplitterClass;
typedef struct _UfoFilterSplitterPrivate    UfoFilterSplitterPrivate;

/**
 * UfoFilterSplitter:
 *
 * The contents of this object is opaque to the user.
 */
struct _UfoFilterSplitter {
    /*< private >*/
    UfoFilter parent;

    UfoFilterSplitterPrivate *priv;
};

/**
 * UfoFilterSplitterClass:
 * @parent: the parent class
 */
struct _UfoFilterSplitterClass {
    UfoFilterClass parent;
};

GType    ufo_filter_splitter_get_type   (void);

G_END_DECLS

#endif
