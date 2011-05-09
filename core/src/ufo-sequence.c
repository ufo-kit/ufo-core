#include "ufo-sequence.h"
#include "ufo-element.h"

struct _UfoSequencePrivate {
    GList *children;

    /* XXX: In fact we don't need those two queues, because the input of a
     * sequence corresponds to the input of the very first child and the output
     * with the output of the very last child. So, in the future we might
     * respect this fact and drop these queues. */
    GAsyncQueue *input_queue;
    GAsyncQueue *output_queue;
};

static void ufo_element_iface_init(UfoElementInterface *iface);

G_DEFINE_TYPE_WITH_CODE(UfoSequence, 
                        ufo_sequence, 
                        UFO_TYPE_CONTAINER,
                        G_IMPLEMENT_INTERFACE(UFO_TYPE_ELEMENT,
                                              ufo_element_iface_init));

#define UFO_SEQUENCE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_SEQUENCE, UfoSequencePrivate))


/* 
 * Public Interface
 */
/**
 * \brief Create a new UfoSequence object
 * \public \memberof UfoSequence
 * \return A UfoElement
 */
UfoSequence *ufo_sequence_new()
{
    return UFO_SEQUENCE(g_object_new(UFO_TYPE_SEQUENCE, NULL));
}

/* 
 * Virtual Methods 
 */
static void ufo_sequence_add_element(UfoContainer *container, UfoElement *child)
{
    if (container == NULL || child == NULL)
        return;

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
        prev = self->priv->input_queue;
    }

    /* Ok, we have some old output and connect it to the newly added element */
    ufo_element_set_input_queue(child, prev);

    /* Now, we create a new output that is also going to be the container's
     * real output */
    GAsyncQueue *next = g_async_queue_new();
    ufo_element_set_output_queue(child, next);
    self->priv->output_queue = next;
    self->priv->children = g_list_append(self->priv->children, child);
    g_object_ref(child);
}

static gpointer ufo_sequence_process_thread(gpointer data)
{
    ufo_element_process(UFO_ELEMENT(data));
    return NULL;
}

static void ufo_sequence_process(UfoElement *element)
{
    UfoSequence *self = UFO_SEQUENCE(element);
    GList *threads = NULL;
    GError *error = NULL;
    for (guint i = 0; i < g_list_length(self->priv->children); i++) {
        UfoElement *child = UFO_ELEMENT(g_list_nth_data(self->priv->children, i));
        g_message("[seq:%p] starting element %p", element, child);
        threads = g_list_append(threads,
                g_thread_create(ufo_sequence_process_thread, child, TRUE, &error));
    }
    g_list_foreach(threads, ufo_container_join_threads, NULL);
    g_message("[seq:%p] done", element);
}

static void ufo_sequence_print(UfoElement *element)
{
    UfoSequence *self = UFO_SEQUENCE(element);
    g_message("[seq:%p] <%p,%p>", element, 
            ufo_element_get_input_queue(element),
            ufo_element_get_output_queue(element));
    for (guint i = 0; i < g_list_length(self->priv->children); i++) {
        UfoElement *child = UFO_ELEMENT(g_list_nth_data(self->priv->children, i));
        ufo_element_print(child);
    }
    g_message("[/seq:%p]", element);
}

static void ufo_sequence_set_input_queue(UfoElement *element, GAsyncQueue *queue)
{
    UfoSequence *self = UFO_SEQUENCE(element);
    self->priv->input_queue = queue;
}

static void ufo_sequence_set_output_queue(UfoElement *element, GAsyncQueue *queue)
{
    UfoSequence *self = UFO_SEQUENCE(element);
    self->priv->output_queue = queue;
}

static GAsyncQueue *ufo_sequence_get_input_queue(UfoElement *element)
{
    UfoSequence *self = UFO_SEQUENCE(element);
    return self->priv->input_queue;
}

static GAsyncQueue *ufo_sequence_get_output_queue(UfoElement *element)
{
    UfoSequence *self = UFO_SEQUENCE(element);
    return self->priv->input_queue;
}


/*
 * Type/Class Initialization
 */
static void ufo_element_iface_init(UfoElementInterface *iface)
{
    iface->process = ufo_sequence_process;
    iface->print = ufo_sequence_print;
    iface->set_input_queue = ufo_sequence_set_input_queue;
    iface->set_output_queue = ufo_sequence_set_output_queue;
    iface->get_input_queue = ufo_sequence_get_input_queue;
    iface->get_output_queue = ufo_sequence_get_output_queue;
}

static void ufo_sequence_class_init(UfoSequenceClass *klass)
{
    /* override methods */
    UfoContainerClass *container_class = UFO_CONTAINER_CLASS(klass);
    container_class->add_element = ufo_sequence_add_element;

    /* install private data */
    g_type_class_add_private(klass, sizeof(UfoSequencePrivate));
}

static void ufo_sequence_init(UfoSequence *self)
{
    self->priv = UFO_SEQUENCE_GET_PRIVATE(self);
    self->priv->children = NULL;
    self->priv->input_queue = NULL;
    self->priv->output_queue = NULL;
}
