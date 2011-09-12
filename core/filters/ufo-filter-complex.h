#ifndef __UFO_FILTER_COMPLEX_H
#define __UFO_FILTER_COMPLEX_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

#define UFO_TYPE_FILTER_COMPLEX             (ufo_filter_complex_get_type())
#define UFO_FILTER_COMPLEX(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_COMPLEX, UfoFilterComplex))
#define UFO_IS_FILTER_COMPLEX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_COMPLEX))
#define UFO_FILTER_COMPLEX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_COMPLEX, UfoFilterComplexClass))
#define UFO_IS_FILTER_COMPLEX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_COMPLEX))
#define UFO_FILTER_COMPLEX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_COMPLEX, UfoFilterComplexClass))

typedef struct _UfoFilterComplex           UfoFilterComplex;
typedef struct _UfoFilterComplexClass      UfoFilterComplexClass;
typedef struct _UfoFilterComplexPrivate    UfoFilterComplexPrivate;

struct _UfoFilterComplex {
    UfoFilter parent_instance;

    /* private */
    UfoFilterComplexPrivate *priv;
};

struct _UfoFilterComplexClass {
    UfoFilterClass parent_class;
};

/* virtual public methods */

/* non-virtual public methods */

GType ufo_filter_complex_get_type(void);

#endif
