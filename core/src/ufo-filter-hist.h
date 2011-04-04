#ifndef __UFO_FILTER_HIST_H
#define __UFO_FILTER_HIST_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

#define UFO_TYPE_FILTER_HIST             (ufo_filter_hist_get_type())
#define UFO_FILTER_HIST(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_HIST, UfoFilterHist))
#define UFO_IS_FILTER_HIST(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_HIST))
#define UFO_FILTER_HIST_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_HIST, UfoFilterHistClass))
#define UFO_IS_FILTER_HIST_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_HIST))
#define UFO_FILTER_HIST_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_HIST, UfoFilterHistClass))

typedef struct _UfoFilterHist           UfoFilterHist;
typedef struct _UfoFilterHistClass      UfoFilterHistClass;
typedef struct _UfoFilterHistPrivate    UfoFilterHistPrivate;

struct _UfoFilterHist {
    UfoFilter parent_instance;

    /* public */

    /* private */
    UfoFilterHistPrivate *priv;
};

struct _UfoFilterHistClass {
    UfoFilterClass parent_class;

    /* class members */

    /* virtual public methods */
};

/* virtual public methods */

/* non-virtual public methods */

GType ufo_filter_hist_get_type(void);

#endif
