#ifndef __UFO_CONNECTION_H
#define __UFO_CONNECTION_H

#include <glib.h>
#include <glib-object.h>
#include "ufo-filter.h"

#define UFO_TYPE_CONNECTION             (ufo_connection_get_type())
#define UFO_CONNECTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_CONNECTION, UfoConnection))
#define UFO_IS_CONNECTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_CONNECTION))
#define UFO_CONNECTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_CONNECTION, UfoConnectionClass))
#define UFO_IS_CONNECTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_CONNECTION))
#define UFO_CONNECTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_CONNECTION, UfoConnectionClass))

typedef struct _UfoConnection           UfoConnection;
typedef struct _UfoConnectionClass      UfoConnectionClass;
typedef struct _UfoConnectionPrivate    UfoConnectionPrivate;


struct _UfoConnection {
    GObject parent_instance;

    /* public */

    /* private */
    UfoConnectionPrivate *priv;
};

struct _UfoConnectionClass {
    GObjectClass parent_class;

    /* class members */

    /* virtual public methods */
};

/* virtual public methods */

/* non-virtual public methods */
UfoConnection *ufo_connection_new();
GAsyncQueue *ufo_connection_get_queue(UfoConnection *self);
void ufo_connection_set_filters(UfoConnection *self, UfoFilter *src, UfoFilter *dst);


GType ufo_connection_get_type(void);

#endif
