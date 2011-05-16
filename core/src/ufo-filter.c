#include <glib.h>
#include <gmodule.h>

#include "ufo-filter.h"
#include "ufo-element.h"

static void ufo_element_iface_init(UfoElementInterface *iface);

G_DEFINE_TYPE_WITH_CODE(UfoFilter,
                        ufo_filter, 
                        ETHOS_TYPE_PLUGIN,
                        G_IMPLEMENT_INTERFACE(UFO_TYPE_ELEMENT,
                                              ufo_element_iface_init));

#define UFO_FILTER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER, UfoFilterPrivate))

enum {
    FINISHED,
    LAST_SIGNAL
};

struct _UfoFilterPrivate {
    UfoResourceManager  *resource_manager;
    GAsyncQueue         *input_queue;
    GAsyncQueue         *output_queue;
    gchar               *name;
};


/* 
 * Public Interface
 */
/**
 * \brief Initializes the concrete UfoFilter with a UfoResourceManager. 
 * \public \memberof UfoFilter
 *
 * This is necessary, because we cannot instantiate the object on our own as
 * this is already done by the plugin manager.
 *
 * \param[in] filter A UfoFilter object
 * \param[in] resource_manager A UfoResourceManager object
 */
void ufo_filter_initialize(UfoFilter *filter)
{
    if (UFO_FILTER_GET_CLASS(filter)->initialize != NULL)
        UFO_FILTER_GET_CLASS(filter)->initialize(filter);
}

/**
 * \brief Execute a particular filter.
 * \public \memberof UfoFilter
 *
 * \param filter A UfoFilter object
 */
void ufo_filter_process(UfoFilter *filter)
{
    if (UFO_FILTER_GET_CLASS(filter)->process != NULL)
        UFO_FILTER_GET_CLASS(filter)->process(filter);
}

UfoBuffer *ufo_filter_pop_buffer(UfoFilter *filter)
{
    GAsyncQueue *input_queue = ufo_element_get_input_queue(UFO_ELEMENT(filter));
    return g_async_queue_pop(input_queue);
}

void ufo_filter_push_buffer(UfoFilter *filter, UfoBuffer *buffer)
{
    GAsyncQueue *output_queue = ufo_element_get_input_queue(UFO_ELEMENT(filter));
    g_async_queue_push(output_queue, buffer);
}


/* 
 * Virtual Methods
 */
static void ufo_filter_set_input_queue(UfoElement *element, GAsyncQueue *queue)
{
    UfoFilter *self = UFO_FILTER(element);
    if (queue != NULL)
        g_async_queue_ref(queue);
    self->priv->input_queue = queue;
}

static void ufo_filter_set_output_queue(UfoElement *element, GAsyncQueue *queue)
{
    UfoFilter *self = UFO_FILTER(element);
    if (queue != NULL)
        g_async_queue_ref(queue);
    self->priv->output_queue = queue;
}

static GAsyncQueue *ufo_filter_get_input_queue(UfoElement *element)
{
    UfoFilter *self = UFO_FILTER(element);
    return self->priv->input_queue;
}

static GAsyncQueue *ufo_filter_get_output_queue(UfoElement *element)
{
    UfoFilter *self = UFO_FILTER(element);
    return self->priv->output_queue;
}

static void ufo_filter_iface_process(UfoElement *element)
{
    UfoFilter *self = UFO_FILTER(element);
    ufo_filter_process(self);
}

static void ufo_filter_print(UfoElement *self)
{
    g_message(" [filter:%p] <%p,%p>", self, 
            ufo_element_get_input_queue(self),
            ufo_element_get_output_queue(self));
}

static void ufo_filter_finished(UfoElement *self)
{
    g_message("filter: received finished signal");
}

static void ufo_filter_dispose(GObject *object)
{
    UfoFilter *self = UFO_FILTER(object);
    g_async_queue_unref(self->priv->input_queue);
    g_async_queue_unref(self->priv->output_queue);
    G_OBJECT_CLASS(ufo_filter_parent_class)->dispose(object);
}


/*
 * Type/Class Initialization
 */
static void ufo_element_iface_init(UfoElementInterface *iface)
{
    /* virtual methods */
    iface->process = ufo_filter_iface_process;
    iface->print = ufo_filter_print;
    iface->set_input_queue = ufo_filter_set_input_queue;
    iface->set_output_queue = ufo_filter_set_output_queue;
    iface->get_input_queue = ufo_filter_get_input_queue;
    iface->get_output_queue = ufo_filter_get_output_queue;

    /* signals */
    iface->finished = ufo_filter_finished;
}

static void ufo_filter_class_init(UfoFilterClass *klass)
{
    /* override GObject methods */
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = ufo_filter_dispose;
    klass->initialize = NULL;
    klass->process = NULL;

    /* install private data */
    g_type_class_add_private(klass, sizeof(UfoFilterPrivate));
}

static void ufo_filter_init(UfoFilter *self)
{
    UfoFilterPrivate *priv;
    self->priv = priv = UFO_FILTER_GET_PRIVATE(self);
    priv->input_queue = NULL;
    priv->output_queue = NULL;
}

