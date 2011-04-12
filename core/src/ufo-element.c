#include "ufo-element.h"
#include "ufo-filter.h"

G_DEFINE_TYPE(UfoElement, ufo_element, G_TYPE_OBJECT)

#define UFO_ELEMENT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER, UfoElementPrivate))

enum {
    PROP_0,

    PROP_NAME
};

struct _UfoElementPrivate {
    UfoFilter *filter;
};

/*
 * public non-virtual methods
 */
UfoFilter *ufo_element_get_filter(UfoElement *element)
{
    return element->priv->filter;
}

void ufo_element_set_filter(UfoElement *element, UfoFilter *filter)
{
    element->priv->filter = filter;
}

/* 
 * private methods
 */

/* 
 * virtual methods
 */

/*
 * class and object initialization
 */
static void ufo_element_class_init(UfoElementClass *klass)
{
    /* override GObject methods */

    /* install private data */
    g_type_class_add_private(klass, sizeof(UfoElementPrivate));
}

static void ufo_element_init(UfoElement *self)
{
    /* init public fields */

    /* init private fields */
    UfoElementPrivate *priv;
    self->priv = priv = UFO_ELEMENT_GET_PRIVATE(self);
    priv->filter = NULL;
}

