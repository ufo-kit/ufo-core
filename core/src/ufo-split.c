#include "ufo-split.h"

G_DEFINE_TYPE(UfoSplit, ufo_split, UFO_TYPE_CONTAINER);

#define UFO_SPLIT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_SPLIT, UfoSplitPrivate))

enum {
    PROP_0,

    PROP_MODE
};

typedef enum {
    MODE_RANDOM = 0,
    MODE_ROUND_ROBIN,
    MODE_COPY,
    NUM_MODES
} mode_t;

static const char* MODE_STRINGS[] = {
    "random",
    "round-robin",
    "copy"
};

struct _UfoSplitPrivate {
    GList *children;
    GList *queues;
    mode_t mode;
    guint current_node;
};

/* 
 * Public Interface
 */
/**
 * \brief Create a new UfoSplit object
 * \public \memberof UfoSplit
 * \return A UfoElement
 */
UfoContainer *ufo_split_new()
{
    return UFO_CONTAINER(g_object_new(UFO_TYPE_SPLIT, NULL));
}

/* 
 * Virtual Methods 
 */
static void ufo_split_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoSplit *self = UFO_SPLIT(object);

    switch (property_id) {
        case PROP_MODE:
            for (guint i = 0; i < NUM_MODES; i++) {
                if (g_strcmp0(MODE_STRINGS[i], g_value_get_string(value)) == 0) {
                    self->priv->mode = (mode_t) i;
                    break;
                }
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_split_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoSplit *self = UFO_SPLIT(object);

    switch (property_id) {
        case PROP_MODE:
            g_value_set_string(value, MODE_STRINGS[self->priv->mode]);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_split_add_element(UfoContainer *container, UfoElement *element)
{
    /* Contrary to the sequence container, we have to install an input queue for
     * each new element that is added, that we are going to fill according to
     * the `mode`-property. */    
    GAsyncQueue *queue = g_async_queue_new();
    ufo_element_set_input_queue(element, queue);

    UfoSplit *self = UFO_SPLIT(container);
    self->priv->queues = g_list_append(self->priv->queues, queue);
    self->priv->children = g_list_append(self->priv->children, element);

    /* On the other side, we are re-using the same output queue for all nodes */
    queue = ufo_element_get_output_queue(UFO_ELEMENT(container));
    if (queue == NULL) {
        queue = g_async_queue_new();
        ufo_element_set_output_queue(UFO_ELEMENT(container), queue);
    }
    ufo_element_set_output_queue(element, queue);
}

static void ufo_split_process(UfoElement *element)
{
    UfoSplit *self = UFO_SPLIT(element);

    /* First, start all children */
    GList *current_child = self->priv->children;
    while (current_child != NULL) {
        UfoElement *child = UFO_ELEMENT(current_child->data);
        ufo_element_process(child);
        current_child = g_list_next(current_child);
    }

    /* Then, watch input queue and distribute work */
    GAsyncQueue *input_queue = ufo_element_get_input_queue(element);
    GList *current_queue = self->priv->queues;

    /* TODO: we must finish some time... */
    int i = 0;
    while (i <= 1) {
        /* TODO: replace this round-robin scheme according to the mode */
        UfoBuffer *input = UFO_BUFFER(g_async_queue_pop(input_queue));
        if (input == NULL)
            break;

        g_message("relaying buffer %p to queue %p", input, current_queue->data);
        g_async_queue_push((GAsyncQueue *) current_queue->data, input);

        /* Start from beginning if no more queues */
        current_queue = g_list_next(current_queue);
        if (current_queue == NULL)
            current_queue = self->priv->queues;
        i++;
    }
}

static void ufo_split_print(UfoElement *element)
{
    UfoSplit *self = UFO_SPLIT(element);
    g_message("[split:%p|m:%i] <%p,%p>", element, self->priv->mode,
            ufo_element_get_input_queue(element),
            ufo_element_get_output_queue(element));
    for (guint i = 0; i < g_list_length(self->priv->children); i++) {
        UfoElement *child = UFO_ELEMENT(g_list_nth_data(self->priv->children, i));
        ufo_element_print(child);
    }
    g_message("[/split:%p]", element);
}

static void ufo_split_finished(UfoElement *element)
{
    UfoSplit *self = UFO_SPLIT(element);
    g_message("split: received finished");
    for (guint i = 0; i < g_list_length(self->priv->children); i++) {
        UfoElement *child = UFO_ELEMENT(g_list_nth_data(self->priv->children, i));
        g_signal_emit_by_name(child, "finished");
    }
}

/*
 * Type/Class Initialization
 */
static void ufo_split_class_init(UfoSplitClass *klass)
{
    /* override methods */
    UfoElementClass *element_class = UFO_ELEMENT_CLASS(klass);
    UfoContainerClass *container_class = UFO_CONTAINER_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->set_property = ufo_split_set_property;
    gobject_class->get_property = ufo_split_get_property;
    element_class->process = ufo_split_process;
    element_class->print = ufo_split_print;
    element_class->finished = ufo_split_finished;
    container_class->add_element = ufo_split_add_element;

    /* install properties */
    GParamSpec *pspec;

    pspec = g_param_spec_string("mode",
        "Mode of operation",
        "Control distribution of incoming work to children",
        "round-robin",
        G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_MODE, pspec);

    /* install private data */
    g_type_class_add_private(klass, sizeof(UfoSplitPrivate));
}

static void ufo_split_init(UfoSplit *self)
{
    self->priv = UFO_SPLIT_GET_PRIVATE(self);
    self->priv->mode = MODE_ROUND_ROBIN;
    self->priv->children = NULL;
}
