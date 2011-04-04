#ifndef __UFO_FILTER_RAW_SOURCE_H
#define __UFO_FILTER_RAW_SOURCE_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

#define UFO_TYPE_FILTER_RAW_SOURCE             (ufo_filter_raw_source_get_type())
#define UFO_FILTER_RAW_SOURCE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_RAW_SOURCE, UfoFilterRawSource))
#define UFO_IS_FILTER_RAW_SOURCE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_RAW_SOURCE))
#define UFO_FILTER_RAW_SOURCE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_RAW_SOURCE, UfoFilterRawSourceClass))
#define UFO_IS_FILTER_RAW_SOURCE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_RAW_SOURCE))
#define UFO_FILTER_RAW_SOURCE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_RAW_SOURCE, UfoFilterRawSourceClass))

typedef struct _UfoFilterRawSource           UfoFilterRawSource;
typedef struct _UfoFilterRawSourceClass      UfoFilterRawSourceClass;
typedef struct _UfoFilterRawSourcePrivate    UfoFilterRawSourcePrivate;

struct _UfoFilterRawSource {
    UfoFilter parent_instance;

    /* public */

    /* private */
    UfoFilterRawSourcePrivate *priv;
};

struct _UfoFilterRawSourceClass {
    UfoFilterClass parent_class;

    /* class members */

    /* virtual public methods */
};

/* virtual public methods */

/* non-virtual public methods */

void ufo_filter_raw_source_set_info(UfoFilterRawSource *self, const gchar *filename, gint32 width, gint32 height, gint32 bpp);

GType ufo_filter_raw_source_get_type(void);

#endif
