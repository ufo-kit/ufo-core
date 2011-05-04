#ifndef __UFO_FILTER_BACKPROJECT_H
#define __UFO_FILTER_BACKPROJECT_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

#define UFO_TYPE_FILTER_BACKPROJECT             (ufo_filter_backproject_get_type())
#define UFO_FILTER_BACKPROJECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_BACKPROJECT, UfoFilterBackproject))
#define UFO_IS_FILTER_BACKPROJECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_BACKPROJECT))
#define UFO_FILTER_BACKPROJECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_BACKPROJECT, UfoFilterBackprojectClass))
#define UFO_IS_FILTER_BACKPROJECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_BACKPROJECT))
#define UFO_FILTER_BACKPROJECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_BACKPROJECT, UfoFilterBackprojectClass))

typedef struct _UfoFilterBackproject           UfoFilterBackproject;
typedef struct _UfoFilterBackprojectClass      UfoFilterBackprojectClass;
typedef struct _UfoFilterBackprojectPrivate    UfoFilterBackprojectPrivate;

struct _UfoFilterBackproject {
    UfoFilter parent_instance;

    /* private */
    UfoFilterBackprojectPrivate *priv;
};

struct _UfoFilterBackprojectClass {
    UfoFilterClass parent_class;

    /* virtual public methods */
};

GType ufo_filter_backproject_get_type(void);

#endif
