#ifndef __UFO_FILTER_UCA_H
#define __UFO_FILTER_UCA_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

#define UFO_TYPE_FILTER_UCA             (ufo_filter_uca_get_type())
#define UFO_FILTER_UCA(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_UCA, UfoFilterUCA))
#define UFO_IS_FILTER_UCA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_UCA))
#define UFO_FILTER_UCA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_UCA, UfoFilterUCAClass))
#define UFO_IS_FILTER_UCA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_UCA))
#define UFO_FILTER_UCA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_UCA, UfoFilterUCAClass))

typedef struct _UfoFilterUCA           UfoFilterUCA;
typedef struct _UfoFilterUCAClass      UfoFilterUCAClass;
typedef struct _UfoFilterUCAPrivate    UfoFilterUCAPrivate;

struct _UfoFilterUCA {
    UfoFilter parent_instance;

    /* public */

    /* private */
    UfoFilterUCAPrivate *priv;
};

struct _UfoFilterUCAClass {
    UfoFilterClass parent_class;

    /* class members */

    /* virtual public methods */
};

/* virtual public methods */

/* non-virtual public methods */

GType ufo_filter_uca_get_type(void);

#endif
