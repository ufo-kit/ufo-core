#include "ufo-split.h"
#include "ufo-element.h"

static void ufo_element_iface_init(UfoElementInterface *iface);

G_DEFINE_TYPE_WITH_CODE(UfoSplit,
                        ufo_split, 
                        UFO_TYPE_CONTAINER,
                        G_IMPLEMENT_INTERFACE(UFO_TYPE_ELEMENT,
                                              ufo_element_iface_init));

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
    GAsyncQueue *input_queue;
    GAsyncQueue *output_queue;
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
UfoSplit *ufo_split_new()
{
    return UFO_SPLIT(g_object_new(UFO_TYPE_SPLIT, NULL));
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

static void ufo_split_add_element(UfoContainer *container, UfoElement *child)
{
    /* Contrary to the split container, we have to install an input queue for
     * each new element that is added, that we are going to fill according to
     * the `mode`-property. */    
    GAsyncQueue *queue = g_async_queue_new();
    ufo_element_set_input_queue(child, queue);

    UfoSplit *self = UFO_SPLIT(container);
    self->priv->queues = g_list_append(self->priv->queues, queue);
    self->priv->children = g_list_append(self->priv->children, child);

    /* On the other side, we are re-using the same output queue for all nodes */
    if (self->priv->output_queue == NULL) {
        self->priv->output_queue = g_async_queue_new();
    }
    ufo_element_set_output_queue(child, self->priv->output_queue);
}

static gpointer ufo_split_process_thread(gpointer data)
{
    ufo_element_process(UFO_ELEMENT(data));
    return NULL;
}

static void ufo_split_process(UfoElement *element)
{
    UfoSplit *self = UFO_SPLIT(element);
    GError *error = NULL;

    /* First, start all children */
    GList *current_child = self->priv->children;
    while (current_child != NULL) {
        UfoElement *child = UFO_ELEMENT(current_child->data);
        g_thread_create(ufo_split_process_thread, child, TRUE, &error);
        current_child = g_list_next(current_child);
    }

    /* Then, watch input queue and distribute work */
    GList *current_queue = self->priv->queues;

    int i = 0;
    while (i <= 1) {
        /* TODO: replace this round-robin scheme according to the mode */
        UfoBuffer *input = UFO_BUFFER(g_async_queue_pop(self->priv->input_queue));
        if (input == NULL)
            break;

        /* TODO: when finished == TRUE, we must copy one finished buffer for
         * each child */
        gboolean finished = FALSE;
        g_object_get(input,
                "finished", &finished,
                NULL);

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

static void ufo_split_set_input_queue(UfoElement *element, GAsyncQueue *queue)
{
    UfoSplit *self = UFO_SPLIT(element);
    self->priv->input_queue = queue;
}

static void ufo_split_set_output_queue(UfoElement *element, GAsyncQueue *queue)
{
    UfoSplit *self = UFO_SPLIT(element);
    self->priv->output_queue = queue;
}

static GAsyncQueue *ufo_split_get_input_queue(UfoElement *element)
{
    UfoSplit *self = UFO_SPLIT(element);
    return self->priv->input_queue;
}

static GAsyncQueue *ufo_split_get_output_queue(UfoElement *element)
{
    UfoSplit *self = UFO_SPLIT(element);
    return self->priv->output_queue;
}

/*
 * Type/Class Initialization
 */
static void ufo_element_iface_init(UfoElementInterface *iface)
{
    iface->process = ufo_split_process;
    iface->print = ufo_split_print;
    iface->set_input_queue = ufo_split_set_input_queue;
    iface->set_output_queue = ufo_split_set_output_queue;
    iface->get_input_queue = ufo_split_get_input_queue;
    iface->get_output_queue = ufo_split_get_output_queue;
}

static void ufo_split_class_init(UfoSplitClass *klass)
{
    /* override methods */
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    UfoContainerClass *container_class = UFO_CONTAINER_CLASS(klass);

    gobject_class->set_property = ufo_split_set_property;
    gobject_class->get_property = ufo_split_get_property;
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
    self->priv->output_queue = NULL;
    self->priv->input_queue = NULL;
}
