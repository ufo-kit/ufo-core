#include "ufo-container.h"
#include "ufo-element.h"

G_DEFINE_TYPE(UfoContainer, ufo_container, UFO_TYPE_ELEMENT);

#define UFO_CONTAINER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CONTAINER, UfoContainerPrivate))

enum {
    PROP_0,

    PROP_PIPELINED
};

struct _UfoContainerPrivate {
    GList *children;
    gboolean pipelined;
};


/* 
 * non-virtual public methods 
 */

UfoContainer *ufo_container_new()
{
    return g_object_new(UFO_TYPE_CONTAINER, NULL);
}

/* 
 * virtual methods 
 */
void ufo_container_add_element(UfoContainer *self, UfoElement *element)
{
    /* In this method we also need to add the asynchronous queues. It is
     * important to understand the two cases:
     * 
     * 1. There is no element in the list. Then we just add the new element
     * with new input_queue as input_queue from container and no output.
     *
     * 2. There is an element in the list. Then we try to get that old element's
     * output queue.
     */
    GList *last = g_list_last(self->priv->children);
    GAsyncQueue *prev = NULL;

    if (last != NULL) {
        /* We have the last element. Use its output as the input to the
         * next element */
        UfoElement *last_element = (UfoElement *) last->data;
        prev = ufo_element_get_output_queue(last_element);
    }
    else {
        /* We have no children, so use the container's input as the input to the
         * next element */
        prev = ufo_element_get_input_queue((UfoElement *) self);
    }

    /* Ok, we have some old output and connect it to the newly added element */
    ufo_element_set_input_queue(element, prev);

    /* Now, we create a new output that is also going to be the container's
     * real output */
    GAsyncQueue *next = g_async_queue_new();
    ufo_element_set_output_queue(element, next);
    ufo_element_set_output_queue((UfoElement *) self, next);
    self->priv->children = g_list_append(self->priv->children, element);
}

static void ufo_container_process(UfoElement *element)
{
    UfoContainer *self = (UfoContainer *) element;
    for (guint i = 0; i < g_list_length(self->priv->children); i++) {
        UfoElement *child = (UfoElement *) g_list_nth_data(self->priv->children, i);
        ufo_element_process(child);
    }
}

static void ufo_container_print(UfoElement *element)
{
    UfoContainer *self = (UfoContainer *) element;
    g_message("[node:%p] <%p,%p>", element, ufo_element_get_input_queue(element),
            ufo_element_get_output_queue(element));
    for (guint i = 0; i < g_list_length(self->priv->children); i++) {
        UfoElement *child = (UfoElement *) g_list_nth_data(self->priv->children, i);
        ufo_element_print(child);
    }
    g_message("[/node:%p]", element);
}


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
    UfoElementClass *element_class = UFO_ELEMENT_CLASS(klass);

    /* override methods */
    /* TODO: add dispose/finalize methods */
    gobject_class->set_property = ufo_container_set_property;
    gobject_class->get_property = ufo_container_get_property;
    element_class->process = ufo_container_process;
    element_class->print = ufo_container_print;
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
    priv->children = NULL;
    priv->pipelined = TRUE;
}
