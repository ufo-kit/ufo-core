#ifndef __UFO_FILTER_CL_H
#define __UFO_FILTER_CL_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

#define UFO_TYPE_FILTER_CL             (ufo_filter_cl_get_type())
#define UFO_FILTER_CL(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_CL, UfoFilterCl))
#define UFO_IS_FILTER_CL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_CL))
#define UFO_FILTER_CL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_CL, UfoFilterClClass))
#define UFO_IS_FILTER_CL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_CL))
#define UFO_FILTER_CL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_CL, UfoFilterClClass))

typedef struct _UfoFilterCl           UfoFilterCl;
typedef struct _UfoFilterClClass      UfoFilterClClass;
typedef struct _UfoFilterClPrivate    UfoFilterClPrivate;

struct _UfoFilterCl {
    UfoFilter parent_instance;

    /* private */
    UfoFilterClPrivate *priv;
};

struct _UfoFilterClClass {
    UfoFilterClass parent_class;
};

/* virtual public methods */

/* non-virtual public methods */

GType ufo_filter_cl_get_type(void);

#endif
