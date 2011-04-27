#ifndef __UFO_FILTER_SCALE_H
#define __UFO_FILTER_SCALE_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

#define UFO_TYPE_FILTER_SCALE             (ufo_filter_scale_get_type())
#define UFO_FILTER_SCALE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_SCALE, UfoFilterScale))
#define UFO_IS_FILTER_SCALE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_SCALE))
#define UFO_FILTER_SCALE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_SCALE, UfoFilterScaleClass))
#define UFO_IS_FILTER_SCALE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_SCALE))
#define UFO_FILTER_SCALE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_SCALE, UfoFilterScaleClass))

typedef struct _UfoFilterScale           UfoFilterScale;
typedef struct _UfoFilterScaleClass      UfoFilterScaleClass;
typedef struct _UfoFilterScalePrivate    UfoFilterScalePrivate;

struct _UfoFilterScale {
    UfoFilter parent_instance;

    /* public */

    /* private */
    UfoFilterScalePrivate *priv;
};

struct _UfoFilterScaleClass {
    UfoFilterClass parent_class;

    /* class members */

    /* virtual public methods */
};

/* virtual public methods */

/* non-virtual public methods */

GType ufo_filter_scale_get_type(void);

#endif
