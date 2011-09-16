#ifndef __UFO_FILTER_ARG_MAX_H
#define __UFO_FILTER_ARG_MAX_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

#define UFO_TYPE_FILTER_ARG_MAX             (ufo_filter_arg_max_get_type())
#define UFO_FILTER_ARG_MAX(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_ARG_MAX, UfoFilterArgMax))
#define UFO_IS_FILTER_ARG_MAX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_ARG_MAX))
#define UFO_FILTER_ARG_MAX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_ARG_MAX, UfoFilterArgMaxClass))
#define UFO_IS_FILTER_ARG_MAX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_ARG_MAX))
#define UFO_FILTER_ARG_MAX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_ARG_MAX, UfoFilterArgMaxClass))

typedef struct _UfoFilterArgMax           UfoFilterArgMax;
typedef struct _UfoFilterArgMaxClass      UfoFilterArgMaxClass;
typedef struct _UfoFilterArgMaxPrivate    UfoFilterArgMaxPrivate;

struct _UfoFilterArgMax {
    UfoFilter parent_instance;

    /* private */
    UfoFilterArgMaxPrivate *priv;
};

struct _UfoFilterArgMaxClass {
    UfoFilterClass parent_class;
};

/* virtual public methods */

/* non-virtual public methods */

GType ufo_filter_arg_max_get_type(void);

#endif
