#ifndef __UFO_FILTER_READER_H
#define __UFO_FILTER_READER_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

#define UFO_TYPE_FILTER_READER             (ufo_filter_reader_get_type())
#define UFO_FILTER_READER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_READER, UfoFilterReader))
#define UFO_IS_FILTER_READER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_READER))
#define UFO_FILTER_READER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_READER, UfoFilterReaderClass))
#define UFO_IS_FILTER_READER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_READER))
#define UFO_FILTER_READER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_READER, UfoFilterReaderClass))

typedef struct _UfoFilterReader           UfoFilterReader;
typedef struct _UfoFilterReaderClass      UfoFilterReaderClass;
typedef struct _UfoFilterReaderPrivate    UfoFilterReaderPrivate;

struct _UfoFilterReader {
    UfoFilter parent_instance;

    /* private */
    UfoFilterReaderPrivate *priv;
};

struct _UfoFilterReaderClass {
    UfoFilterClass parent_class;
};

GType ufo_filter_reader_get_type(void);

#endif
