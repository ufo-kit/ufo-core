#ifndef __UFO_FILTER_RAW_H
#define __UFO_FILTER_RAW_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

#define UFO_TYPE_FILTER_RAW             (ufo_filter_raw_get_type())
#define UFO_FILTER_RAW(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_RAW, UfoFilterRaw))
#define UFO_IS_FILTER_RAW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_RAW))
#define UFO_FILTER_RAW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_RAW, UfoFilterRawClass))
#define UFO_IS_FILTER_RAW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_RAW))
#define UFO_FILTER_RAW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_RAW, UfoFilterRawClass))

typedef struct _UfoFilterRaw           UfoFilterRaw;
typedef struct _UfoFilterRawClass      UfoFilterRawClass;
typedef struct _UfoFilterRawPrivate    UfoFilterRawPrivate;

struct _UfoFilterRaw {
    UfoFilter parent_instance;

    /* public */

    /* private */
    UfoFilterRawPrivate *priv;
};

struct _UfoFilterRawClass {
    UfoFilterClass parent_class;

    /* class members */

    /* virtual public methods */
};

/* virtual public methods */

/* non-virtual public methods */

GType ufo_filter_raw_get_type(void);

#endif
