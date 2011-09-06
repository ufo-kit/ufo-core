#ifndef __UFO_FILTER_DEMUX_H
#define __UFO_FILTER_DEMUX_H

#include <glib.h>
#include <glib-object.h>

#include "ufo-filter.h"

#define UFO_TYPE_FILTER_DEMUX             (ufo_filter_demux_get_type())
#define UFO_FILTER_DEMUX(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_DEMUX, UfoFilterDemux))
#define UFO_IS_FILTER_DEMUX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_DEMUX))
#define UFO_FILTER_DEMUX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_DEMUX, UfoFilterDemuxClass))
#define UFO_IS_FILTER_DEMUX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_DEMUX))
#define UFO_FILTER_DEMUX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_DEMUX, UfoFilterDemuxClass))

typedef struct _UfoFilterDemux           UfoFilterDemux;
typedef struct _UfoFilterDemuxClass      UfoFilterDemuxClass;
typedef struct _UfoFilterDemuxPrivate    UfoFilterDemuxPrivate;

struct _UfoFilterDemux {
    UfoFilter parent_instance;

    /* private */
    UfoFilterDemuxPrivate *priv;
};

struct _UfoFilterDemuxClass {
    UfoFilterClass parent_class;
};

/* virtual public methods */

/* non-virtual public methods */

GType ufo_filter_demux_get_type(void);

#endif
