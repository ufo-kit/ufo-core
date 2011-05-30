#ifndef __UFO_FILTER_CIRCLE_CROP_H
#define __UFO_FILTER_CIRCLE_CROP_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

#define UFO_TYPE_FILTER_CIRCLE_CROP             (ufo_filter_circle_crop_get_type())
#define UFO_FILTER_CIRCLE_CROP(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_CIRCLE_CROP, UfoFilterCircleCrop))
#define UFO_IS_FILTER_CIRCLE_CROP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_CIRCLE_CROP))
#define UFO_FILTER_CIRCLE_CROP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_CIRCLE_CROP, UfoFilterCircleCropClass))
#define UFO_IS_FILTER_CIRCLE_CROP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_CIRCLE_CROP))
#define UFO_FILTER_CIRCLE_CROP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_CIRCLE_CROP, UfoFilterCircleCropClass))

typedef struct _UfoFilterCircleCrop           UfoFilterCircleCrop;
typedef struct _UfoFilterCircleCropClass      UfoFilterCircleCropClass;
typedef struct _UfoFilterCircleCropPrivate    UfoFilterCircleCropPrivate;

struct _UfoFilterCircleCrop {
    UfoFilter parent_instance;

    /* private */
    UfoFilterCircleCropPrivate *priv;
};

struct _UfoFilterCircleCropClass {
    UfoFilterClass parent_class;
};

/* virtual public methods */

/* non-virtual public methods */

GType ufo_filter_circle_crop_get_type(void);

#endif
