#include "ufo-container.h"
#include "ufo-element.h"
#include "ufo-connection.h"

G_DEFINE_TYPE(UfoContainer, ufo_container, UFO_TYPE_ELEMENT);

#define UFO_CONTAINER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CONTAINER, UfoContainerPrivate))

enum {
    PROP_0,

    PROP_PIPELINED
};

struct _UfoContainerPrivate {
    GList *elements;
    GList *connections;
    gboolean pipelined;
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
static void ufo_container_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoContainer *self = (UfoContainer *) object;

    switch (property_id) {
        case PROP_PIPELINED:
            self->priv->pipelined = g_value_get_boolean(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_container_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoContainer *self = (UfoContainer *) object;

    switch (property_id) {
        case PROP_PIPELINED:
            g_value_set_boolean(value, self->priv->pipelined);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_container_class_init(UfoContainerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    /* override methods */
    gobject_class->set_property = ufo_container_set_property;
    gobject_class->get_property = ufo_container_get_property;
    klass->add_element = ufo_container_add_element;

    /* install properties */
    GParamSpec *pspec;

    pspec = g_param_spec_boolean("pipelined",
        "Pipelined Mode",
        "Specify if container is executed in true overlapping pipelining mode",
        TRUE,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
    g_object_class_install_property(gobject_class, PROP_PIPELINED, pspec);

    /* install private data */
    g_type_class_add_private(klass, sizeof(UfoContainerPrivate));
}

static void ufo_container_init(UfoContainer *self)
{
    /* init public fields */

    /* init private fields */
    UfoContainerPrivate *priv;
    self->priv = priv = UFO_CONTAINER_GET_PRIVATE(self);
    priv->elements = NULL;
    priv->connections = NULL;
    priv->pipelined = TRUE;
}
