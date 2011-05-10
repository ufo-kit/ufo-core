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
    if (container == NULL || child == NULL)
        return;

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
    UfoSplitPrivate *priv = UFO_SPLIT_GET_PRIVATE(self);
    GError *error = NULL;

    /* First, start all children */
    GList *threads = NULL;
    GList *current_child = self->priv->children;
    while (current_child != NULL) {
        UfoElement *child = UFO_ELEMENT(current_child->data);
        g_message("[split:%p] starting element %p", element, child);
        threads = g_list_append(threads,
                g_thread_create(ufo_split_process_thread, child, TRUE, &error));
        current_child = g_list_next(current_child);
    }

    GList *current_queue = self->priv->queues;
    gboolean finished = FALSE;

    while (!finished) {
        UfoBuffer *input = UFO_BUFFER(g_async_queue_pop(priv->input_queue));
        g_message("[split:%p] received buffer %p at queue %p", self, input, priv->input_queue);

        /* If we receive the finishing buffer, we switch to copy mode to inform
         * all succeeding filters of the end */
        if (ufo_buffer_is_finished(input)) {
            priv->mode = MODE_COPY;
            finished = TRUE;
        }

        /* FIXME: we could also sub-class UfoSplit to different modes of
         * operation... */
        switch (priv->mode) {
            case MODE_RANDOM:
            case MODE_ROUND_ROBIN: 
                {
                    g_message("relaying buffer %p to queue %p", input, current_queue->data);
                    g_async_queue_push((GAsyncQueue *) current_queue->data, input);

                    /* Start from beginning if no more queues */
                    current_queue = g_list_next(current_queue);
                    if (current_queue == NULL)
                        current_queue = self->priv->queues;
                }
                break;

            case MODE_COPY: 
                {
                    /* Create list with copies */
                    GList *copies = NULL;
                    copies = g_list_append(copies, (gpointer) input);
                    int n = g_list_length(priv->queues) - 1;
                    while (n-- > 0) {
                        UfoBuffer *copy = NULL;
                        if (finished)
                            copy = ufo_resource_manager_request_finish_buffer(ufo_resource_manager());
                        else
                            copy = ufo_resource_manager_copy_buffer(ufo_resource_manager(), input);
                        copies = g_list_append(copies, (gpointer) copy);
                    }

                    /* Distribute copies to all attached queues */
                    GList *queue = priv->queues;
                    GList *copy  = copies;
                    while (queue != NULL) {
                        g_assert(copy != NULL);
                        g_message("[split:%p] send buffer %p to queue %p", self, copy->data, queue->data);
                        g_async_queue_push((GAsyncQueue *) queue->data, (UfoBuffer *) copy->data);
                        queue = g_list_next(queue);
                        copy = g_list_next(copy);
                    }
                }
                break; 

            default:
                break;
        }
    }
    /* We cannot just return because we cannot destroy all filters until they
     * are ready */
    g_list_foreach(threads, ufo_container_join_threads, NULL);
    g_list_free(threads);
    g_message("[split:%p] done", element);
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
