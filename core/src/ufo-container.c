#include "ufo-container.h"
#include "ufo-element.h"
#include "ufo-connection.h"

G_DEFINE_TYPE(UfoContainer, ufo_container, UFO_TYPE_ELEMENT);

#define UFO_CONTAINER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CONTAINER, UfoContainerPrivate))


struct _UfoContainerPrivate {
    GList *elements;
    GList *connections;
};


/* 
 * non-virtual public methods 
 */

UfoContainer *ufo_container_new()
{
    return g_object_new(UFO_TYPE_CONTAINER, NULL);
}

void ufo_container_add_element(UfoContainer *self, UfoElement *element)
{
    GList *last = g_list_last(self->priv->elements);
    /* Add a connection and therefore an asynchronous queue between elements, if
     * we have at least one */
    if (last != NULL) {
        UfoConnection *connection = ufo_connection_new();
        ufo_connection_set_elements(connection, (UfoElement *) last->data, element);
        self->priv->connections = g_list_append(self->priv->connections, connection);
    }
    self->priv->elements = g_list_append(self->priv->elements, element);
}

/* 
 * virtual methods 
 */
static void ufo_container_class_init(UfoContainerClass *klass)
{
    g_type_class_add_private(klass, sizeof(UfoContainerPrivate));

    klass->add_element = ufo_container_add_element;
}

static void ufo_container_init(UfoContainer *self)
{
    /* init public fields */

    /* init private fields */
    UfoContainerPrivate *priv;
    self->priv = priv = UFO_CONTAINER_GET_PRIVATE(self);
    priv->elements = NULL;
    priv->connections = NULL;
}
