#ifndef __UFO_FILTER_SOURCE_DIRECT_H
#define __UFO_FILTER_SOURCE_DIRECT_H

#include <glib-object.h>

#include "ufo-buffer.h"
#include "ufo-filter-source.h"

G_BEGIN_DECLS

#define UFO_TYPE_FILTER_SOURCE_DIRECT             (ufo_filter_source_direct_get_type())
#define UFO_FILTER_SOURCE_DIRECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_SOURCE_DIRECT, UfoFilterSourceDirect))
#define UFO_IS_FILTER_SOURCE_DIRECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_SOURCE_DIRECT))
#define UFO_FILTER_SOURCE_DIRECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_SOURCE_DIRECT, UfoFilterSourceDirectClass))
#define UFO_IS_FILTER_SOURCE_DIRECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_SOURCE_DIRECT))
#define UFO_FILTER_SOURCE_DIRECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_SOURCE_DIRECT, UfoFilterSourceDirectClass))

typedef struct _UfoFilterSourceDirect           UfoFilterSourceDirect;
typedef struct _UfoFilterSourceDirectClass      UfoFilterSourceDirectClass;
typedef struct _UfoFilterSourceDirectPrivate    UfoFilterSourceDirectPrivate;

/**
 * UfoFilterSourceDirect:
 *
 * The contents of this object is opaque to the user.
 */
struct _UfoFilterSourceDirect {
    /*< private >*/
    UfoFilterSource parent;

    UfoFilterSourceDirectPrivate *priv;
};

/**
 * UfoFilterSourceDirectClass:
 *
 * @parent: the parent class
 */
struct _UfoFilterSourceDirectClass {
    UfoFilterSourceClass parent;
};

void    ufo_filter_source_direct_push       (UfoFilterSourceDirect  *filter,
                                             UfoBuffer              *buffer);
void    ufo_filter_source_direct_stop       (UfoFilterSourceDirect  *filter);
GType   ufo_filter_source_direct_get_type   (void);

G_END_DECLS

#endif
