#ifndef __UFO_FILTER_MUX_H
#define __UFO_FILTER_MUX_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

#define UFO_TYPE_FILTER_MUX             (ufo_filter_mux_get_type())
#define UFO_FILTER_MUX(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_MUX, UfoFilterMux))
#define UFO_IS_FILTER_MUX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_MUX))
#define UFO_FILTER_MUX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_MUX, UfoFilterMuxClass))
#define UFO_IS_FILTER_MUX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_MUX))
#define UFO_FILTER_MUX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_MUX, UfoFilterMuxClass))

typedef struct _UfoFilterMux           UfoFilterMux;
typedef struct _UfoFilterMuxClass      UfoFilterMuxClass;
typedef struct _UfoFilterMuxPrivate    UfoFilterMuxPrivate;

struct _UfoFilterMux {
    UfoFilter parent_instance;

    /* private */
    UfoFilterMuxPrivate *priv;
};

struct _UfoFilterMuxClass {
    UfoFilterClass parent_class;
};

/* virtual public methods */

/* non-virtual public methods */

GType ufo_filter_mux_get_type(void);

#endif
