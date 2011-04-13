#include "ufo-sequence.h"

G_DEFINE_TYPE(UfoSequence, ufo_sequence, UFO_TYPE_CONTAINER);

#define UFO_SEQUENCE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_SEQUENCE, UfoSequencePrivate))


struct _UfoSequencePrivate {
    GList *children;
};

/* 
 * Public Interface
 */
UfoContainer *ufo_sequence_new()
{
    return UFO_CONTAINER(g_object_new(UFO_TYPE_SEQUENCE, NULL));
}

/* 
 * Virtual Methods 
 */
void ufo_sequence_add_element(UfoContainer *container, UfoElement *element)
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
    UfoSequence *self = UFO_SEQUENCE(container);
    GList *last = g_list_last(self->priv->children);
    GAsyncQueue *prev = NULL;

    if (last != NULL) {
        /* We have the last element. Use its output as the input to the
         * next element */
        UfoElement *last_element = UFO_ELEMENT(last->data);
        prev = ufo_element_get_output_queue(last_element);
    }
    else {
        /* We have no children, so use the container's input as the input to the
         * next element */
        prev = ufo_element_get_input_queue(UFO_ELEMENT(self));
    }

    /* Ok, we have some old output and connect it to the newly added element */
    ufo_element_set_input_queue(element, prev);

    /* Now, we create a new output that is also going to be the container's
     * real output */
    GAsyncQueue *next = g_async_queue_new();
    ufo_element_set_output_queue(element, next);
    ufo_element_set_output_queue(UFO_ELEMENT(self), next);
    self->priv->children = g_list_append(self->priv->children, element);
}

static void ufo_sequence_process(UfoElement *element)
{
    /*UfoContainer *container = UFO_CONTAINER(element);*/
    UfoSequence *self = UFO_SEQUENCE(element);
    for (guint i = 0; i < g_list_length(self->priv->children); i++) {
        UfoElement *child = UFO_ELEMENT(g_list_nth_data(self->priv->children, i));
        ufo_element_process(child);
    }
}

static void ufo_sequence_print(UfoElement *element)
{
    UfoSequence *self = UFO_SEQUENCE(element);
    g_message("[seq:%p] <%p,%p>", element, ufo_element_get_input_queue(element),
            ufo_element_get_output_queue(element));
    for (guint i = 0; i < g_list_length(self->priv->children); i++) {
        UfoElement *child = UFO_ELEMENT(g_list_nth_data(self->priv->children, i));
        ufo_element_print(child);
    }
    g_message("[/seq:%p]", element);
}


/*
 * Type/Class Initialization
 */
static void ufo_sequence_class_init(UfoSequenceClass *klass)
{
    /* override methods */
    UfoElementClass *element_class = UFO_ELEMENT_CLASS(klass);
    UfoContainerClass *container_class = UFO_CONTAINER_CLASS(klass);

    element_class->process = ufo_sequence_process;
    element_class->print = ufo_sequence_print;
    container_class->add_element = ufo_sequence_add_element;

    /* install private data */
    g_type_class_add_private(klass, sizeof(UfoSequencePrivate));
}

static void ufo_sequence_init(UfoSequence *self)
{
    self->priv = UFO_SEQUENCE_GET_PRIVATE(self);
    self->priv->children = NULL;
}
