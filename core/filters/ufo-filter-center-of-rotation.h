#ifndef __UFO_FILTER_CENTER_OF_ROTATION_H
#define __UFO_FILTER_CENTER_OF_ROTATION_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

#define UFO_TYPE_FILTER_CENTER_OF_ROTATION             (ufo_filter_center_of_rotation_get_type())
#define UFO_FILTER_CENTER_OF_ROTATION(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_CENTER_OF_ROTATION, UfoFilterCenterOfRotation))
#define UFO_IS_FILTER_CENTER_OF_ROTATION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_CENTER_OF_ROTATION))
#define UFO_FILTER_CENTER_OF_ROTATION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_CENTER_OF_ROTATION, UfoFilterCenterOfRotationClass))
#define UFO_IS_FILTER_CENTER_OF_ROTATION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_CENTER_OF_ROTATION))
#define UFO_FILTER_CENTER_OF_ROTATION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_CENTER_OF_ROTATION, UfoFilterCenterOfRotationClass))

typedef struct _UfoFilterCenterOfRotation           UfoFilterCenterOfRotation;
typedef struct _UfoFilterCenterOfRotationClass      UfoFilterCenterOfRotationClass;
typedef struct _UfoFilterCenterOfRotationPrivate    UfoFilterCenterOfRotationPrivate;

struct _UfoFilterCenterOfRotation {
    UfoFilter parent_instance;

    /* private */
    UfoFilterCenterOfRotationPrivate *priv;
};

struct _UfoFilterCenterOfRotationClass {
    UfoFilterClass parent_class;
};

/* virtual public methods */

/* non-virtual public methods */

GType ufo_filter_center_of_rotation_get_type(void);

#endif
