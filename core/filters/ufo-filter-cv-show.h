#ifndef __UFO_FILTER_CV_SHOW_H
#define __UFO_FILTER_CV_SHOW_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

#define UFO_TYPE_FILTER_CV_SHOW             (ufo_filter_cv_show_get_type())
#define UFO_FILTER_CV_SHOW(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_CV_SHOW, UfoFilterCvShow))
#define UFO_IS_FILTER_CV_SHOW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_CV_SHOW))
#define UFO_FILTER_CV_SHOW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_CV_SHOW, UfoFilterCvShowClass))
#define UFO_IS_FILTER_CV_SHOW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_CV_SHOW))
#define UFO_FILTER_CV_SHOW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_CV_SHOW, UfoFilterCvShowClass))

typedef struct _UfoFilterCvShow           UfoFilterCvShow;
typedef struct _UfoFilterCvShowClass      UfoFilterCvShowClass;
typedef struct _UfoFilterCvShowPrivate    UfoFilterCvShowPrivate;

struct _UfoFilterCvShow {
    UfoFilter parent_instance;

    /* private */
    UfoFilterCvShowPrivate *priv;
};

struct _UfoFilterCvShowClass {
    UfoFilterClass parent_class;
};

/* virtual public methods */

/* non-virtual public methods */

GType ufo_filter_cv_show_get_type(void);

#endif
