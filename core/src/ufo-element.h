#ifndef __UFO_ELEMENT_H
#define __UFO_ELEMENT_H

#include <glib-object.h>
#include <ethos/ethos.h>

#include "ufo-filter.h"

#define UFO_TYPE_ELEMENT             (ufo_element_get_type())
#define UFO_ELEMENT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_ELEMENT, UfoElement))
#define UFO_IS_ELEMENT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_ELEMENT))
#define UFO_ELEMENT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_ELEMENT, UfoElementClass))
#define UFO_IS_ELEMENT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_ELEMENT))
#define UFO_ELEMENT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_ELEMENT, UfoElementClass))

typedef struct _UfoElement           UfoElement;
typedef struct _UfoElementClass      UfoElementClass;
typedef struct _UfoElementPrivate    UfoElementPrivate;

struct _UfoElement {
    GObject parent;

    /* public */

    /* private */
    UfoElementPrivate *priv;
};

struct _UfoElementClass {
    GObjectClass parent_class;

    /* class members */

    /* virtual public methods */
};

/* non-virtual public methods */
UfoFilter *ufo_element_get_filter(UfoElement *element);
void ufo_element_set_filter(UfoElement *element, UfoFilter *filter);

GType ufo_element_get_type(void);

#endif
