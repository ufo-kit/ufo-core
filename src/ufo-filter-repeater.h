#ifndef __UFO_FILTER_REPEATER_H
#define __UFO_FILTER_REPEATER_H

#include <glib-object.h>

#include "ufo-filter.h"

G_BEGIN_DECLS

#define UFO_TYPE_FILTER_REPEATER             (ufo_filter_repeater_get_type())
#define UFO_FILTER_REPEATER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_REPEATER, UfoFilterRepeater))
#define UFO_IS_FILTER_REPEATER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_REPEATER))
#define UFO_FILTER_REPEATER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_REPEATER, UfoFilterRepeaterClass))
#define UFO_IS_FILTER_REPEATER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_REPEATER))
#define UFO_FILTER_REPEATER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_REPEATER, UfoFilterRepeaterClass))

typedef struct _UfoFilterRepeater           UfoFilterRepeater;
typedef struct _UfoFilterRepeaterClass      UfoFilterRepeaterClass;
typedef struct _UfoFilterRepeaterPrivate    UfoFilterRepeaterPrivate;

/**
 * UfoFilterRepeater:
 *
 * The contents of this object is opaque to the user.
 */
struct _UfoFilterRepeater {
    /*< private >*/
    UfoFilter parent;

    UfoFilterRepeaterPrivate *priv;
};

/**
 * UfoFilterRepeaterClass:
 * @parent: the parent class
 */
struct _UfoFilterRepeaterClass {
    UfoFilterClass parent;
};

GType    ufo_filter_repeater_get_type   (void);

G_END_DECLS

#endif
