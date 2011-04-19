#include "ufo-element.h"
#include "ufo-filter.h"

G_DEFINE_TYPE(UfoElement, ufo_element, G_TYPE_OBJECT)

#define UFO_ELEMENT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_ELEMENT, UfoElementPrivate))

enum {
    FINISHED,
    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_NAME
};

struct _UfoElementPrivate {
    GAsyncQueue *input_queue;   /**< \private \memberof UfoElement */
    GAsyncQueue *output_queue;
    UfoFilter *filter;
};

static guint element_signals[LAST_SIGNAL] = { 0 };

/* 
 * Public Interface
 */

/**
 * \brief Creates a new UfoElement object
 * \public \memberof UfoElement
 * \return An UfoElement
 */
UfoElement *ufo_element_new()
{
    return UFO_ELEMENT(g_object_new(UFO_TYPE_ELEMENT, NULL));
}

/**
 * \brief Return an associated UfoFilter object
 * \public \memberof UfoElement
 * \param[in] element The element whose associated filter is returned
 * \return The associated filter or NULL if there is none like for an UfoSplit
 *      or UfoSequence
 */
UfoFilter *ufo_element_get_filter(UfoElement *element)
{
    return element->priv->filter;
}

/**
 * \brief Associate an UfoElement with a UfoFilter so that an UfoElement
 *      essentially becomes a leaf node.
 * \public \memberof UfoElement
 * \param[in] element an UfoElement
 * \param[in] filter a UfoFilter that is going to be 
 * \return The associated filter or NULL if there is none like for an UfoSplit
 *      or UfoSequence
 */
void ufo_element_set_filter(UfoElement *element, UfoFilter *filter)
{
    g_object_ref(filter);

    /* Now this is tricky: We are subscribing to the child filters "finished"
     * signal and relay it to our own class method that handles "finished" */
    g_message("finished %p", UFO_ELEMENT_GET_CLASS(element)->finished);
    g_signal_connect(filter, "finished",
            G_CALLBACK(UFO_ELEMENT_GET_CLASS(element)->finished), element);

    element->priv->filter = filter;
}

/**
 * \brief Set an input queue
 * \public \memberof UfoElement
 * \param[in] element UfoElement whose input queue is changed
 * \param[in] queue a GAsyncQueue
 */
void ufo_element_set_input_queue(UfoElement *element, GAsyncQueue *queue)
{
    if (queue != NULL) {
        if (element->priv->filter)
            ufo_filter_set_input_queue(element->priv->filter, queue);
        g_async_queue_ref(queue);
    }
    element->priv->input_queue = queue;
}

/**
 * \brief Set an output queue
 * \public \memberof UfoElement
 * \param[in] element UfoElement whose output queue is changed
 * \param[in] queue a GAsyncQueue
 */
void ufo_element_set_output_queue(UfoElement *element, GAsyncQueue *queue)
{
    if (queue != NULL) {
        if (element->priv->filter)
            ufo_filter_set_output_queue(element->priv->filter, queue);
        g_async_queue_ref(queue);
    }
    element->priv->output_queue = queue;
}

/**
 * \brief Get the input queue
 * \public \memberof UfoElement
 * \param[in] element UfoElement whose output queue returned
 * \return A GAsyncQueue
 */
GAsyncQueue *ufo_element_get_input_queue(UfoElement *element)
{
    if (element->priv->filter != NULL)
        return ufo_filter_get_input_queue(element->priv->filter);

    return element->priv->input_queue;
}

/**
 * \brief Get the output queue
 * \public \memberof UfoElement
 * \param[in] element UfoElement whose output queue returned
 * \return A GAsyncQueue
 */
GAsyncQueue *ufo_element_get_output_queue(UfoElement *element)
{
    if (element->priv->filter != NULL)
        return ufo_filter_get_output_queue(element->priv->filter);
    return element->priv->output_queue;
}


/* 
 * Virtual Methods
 */
/**
 * \brief Execute an element
 *
 * Processing an UfoElement means to process an associated UfoFilter or the
 * children of UfoSplit or UfoSequence.
 *
 * \public \memberof UfoElement
 * \param[in] element An UfoElement to process
 */
void ufo_element_process(UfoElement *element)
{
    g_return_if_fail(UFO_IS_ELEMENT(element));
    UFO_ELEMENT_GET_CLASS(element)->process(element);
}

static void ufo_element_dispose(GObject *object)
{
    UfoElement *self = UFO_ELEMENT(object);
    if (self->priv->input_queue)
        g_async_queue_unref(self->priv->input_queue);
    if (self->priv->output_queue)
        g_async_queue_unref(self->priv->output_queue);
    if (self->priv->filter)
        g_object_unref(self->priv->filter);
}

static gpointer ufo_filter_thread(gpointer data)
{
    ufo_filter_process(UFO_FILTER(data));
    return NULL;
}

static void ufo_element_process_default(UfoElement *self)
{
    /* TODO: instead of calling, start as thread */
    if (self->priv->filter != NULL) {
        GError *error = NULL;
        g_thread_create(ufo_filter_thread, self->priv->filter, FALSE, &error);
        if (error) {
            g_message("Error starting thread: %s", error->message);
            g_error_free(error);
        }
    }
}

void ufo_element_print(UfoElement *self)
{
    g_return_if_fail(UFO_IS_ELEMENT(self));
    UFO_ELEMENT_GET_CLASS(self)->print(self);
}

static void ufo_element_print_default(UfoElement *self)
{
    if (self->priv->filter != NULL) {
        g_message("[filter:%p] <%p,%p>",
            self,
            ufo_filter_get_input_queue(self->priv->filter),
            ufo_filter_get_output_queue(self->priv->filter));
    }
}


/*
 * Type/Class Initialization
 */
static void ufo_element_class_init(UfoElementClass *klass)
{
    /* override methods */
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = ufo_element_dispose;
    klass->process = ufo_element_process_default;
    klass->print = ufo_element_print_default;
    klass->finished = NULL;

    /* install signals */
    element_signals[FINISHED] =
        g_signal_new("finished",
                G_TYPE_FROM_CLASS(klass),
                G_SIGNAL_RUN_FIRST,
                G_STRUCT_OFFSET(UfoElementClass, finished),
                NULL, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0, NULL);

    /* install private data */
    g_type_class_add_private(klass, sizeof(UfoElementPrivate));
}

static void ufo_element_init(UfoElement *self)
{
    UfoElementPrivate *priv;
    self->priv = priv = UFO_ELEMENT_GET_PRIVATE(self);
    priv->filter = NULL;
    priv->input_queue = NULL;
    priv->output_queue = NULL;
}

