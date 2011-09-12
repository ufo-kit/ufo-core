#include <glib.h>
#include <gmodule.h>
#include <CL/cl.h>

#include "config.h"
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
    GHashTable          *output_queues; /**< Map from char* to GAsyncQueue * */
    GHashTable          *input_queues;  /**< Map from char* to GAsyncQueue * */
    cl_command_queue    command_queue;
    gchar               *plugin_name;
    float cpu_time;
    float gpu_time;
};

static void filter_set_output_queue(UfoFilter *self, const gchar *name, GAsyncQueue *queue)
{
    GAsyncQueue *old_queue = g_hash_table_lookup(self->priv->output_queues, name);
    if (old_queue != NULL)
        g_hash_table_remove(self->priv->output_queues, name);

    g_hash_table_insert(self->priv->output_queues, g_strdup(name), queue);
    g_async_queue_ref(queue);
}

static void filter_set_input_queue(UfoFilter *self, const gchar *name, GAsyncQueue *queue)
{
    GAsyncQueue *old_queue = g_hash_table_lookup(self->priv->input_queues, name);
    if (old_queue != NULL)
        g_hash_table_remove(self->priv->input_queues, name);

    g_hash_table_insert(self->priv->input_queues, g_strdup(name), queue);
    g_async_queue_ref(queue);
}


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
 */
void ufo_filter_initialize(UfoFilter *filter, const gchar *plugin_name)
{
    UfoFilterPrivate *priv = UFO_FILTER_GET_PRIVATE(filter);
    priv->plugin_name = g_strdup(plugin_name);
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
    if (UFO_FILTER_GET_CLASS(filter)->process != NULL) {
        GTimer *timer = g_timer_new();
        UFO_FILTER_GET_CLASS(filter)->process(filter);
        g_timer_stop(timer);
        filter->priv->cpu_time = g_timer_elapsed(timer, NULL);
        g_timer_destroy(timer);
    }
}

void ufo_filter_connect_to(UfoFilter *source, UfoFilter *destination)
{
    ufo_filter_connect_by_name(source, "default", destination, "default");
}

void ufo_filter_connect_by_name(UfoFilter *source, const gchar *source_output, UfoFilter *destination, const gchar *dest_input)
{
    GAsyncQueue *queue_in = ufo_filter_get_input_queue_by_name(destination, dest_input); 
    GAsyncQueue *queue_out = ufo_filter_get_output_queue_by_name(source, source_output);
    
    if ((queue_in == NULL) && (queue_out == NULL)) {
        GAsyncQueue *queue = g_async_queue_new();
        filter_set_output_queue(source, source_output, queue);
        filter_set_input_queue(destination, dest_input, queue);
    }
    else if (queue_in == NULL)
        filter_set_input_queue(destination, dest_input, queue_out); 
    else if (queue_out == NULL)
        filter_set_output_queue(source, source_output, queue_in); 
}

GAsyncQueue *ufo_filter_get_input_queue_by_name(UfoFilter *filter, const gchar *name)
{
    GAsyncQueue *queue = g_hash_table_lookup(filter->priv->input_queues, name);
    return queue;
}

GAsyncQueue *ufo_filter_get_output_queue_by_name(UfoFilter *filter, const gchar *name)
{
    GAsyncQueue *queue = g_hash_table_lookup(filter->priv->output_queues, name);
    return queue;
}

void ufo_filter_account_gpu_time(UfoFilter *filter, void **event)
{
#ifdef WITH_PROFILING
    cl_ulong start, end;
    cl_event *e = (cl_event *) event;
    clWaitForEvents(1, e);
    clGetEventProfilingInfo(*e, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
    clGetEventProfilingInfo(*e, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
    filter->priv->gpu_time += (end - start) * 1e-9;
#endif
}

float ufo_filter_get_gpu_time(UfoFilter *filter)
{
    return filter->priv->gpu_time;
}

const gchar *ufo_filter_get_plugin_name(UfoFilter *filter)
{
    return filter->priv->plugin_name;
}


/* 
 * Virtual Methods
 */

static void ufo_filter_set_output_queue(UfoElement *element, GAsyncQueue *queue)
{
    /* filter_set_output_queue(element, queue, "default"); */
    g_message("deprecated");
}

static GAsyncQueue *ufo_filter_get_input_queue(UfoElement *element)
{
    UfoFilter *self = UFO_FILTER(element);
    GAsyncQueue *queue = g_hash_table_lookup(self->priv->input_queues, "default");
    return queue;
}

static GAsyncQueue *ufo_filter_get_output_queue(UfoElement *element)
{
    UfoFilter *self = UFO_FILTER(element);
    GAsyncQueue *queue = g_hash_table_lookup(self->priv->output_queues, "default");
    return queue;
}

static void ufo_filter_set_command_queue(UfoElement *element, gpointer command_queue)
{
    UfoFilter *self = UFO_FILTER(element);
    self->priv->command_queue = command_queue;
}

static gpointer ufo_filter_get_command_queue(UfoElement *element)
{
    UfoFilter *self = UFO_FILTER(element);
    return self->priv->command_queue;
}

static float ufo_filter_get_time_spent(UfoElement *element)
{
    UfoFilter *self = UFO_FILTER(element);
    return self->priv->cpu_time;
}

static void ufo_filter_iface_process(UfoElement *element)
{
    UfoFilter *self = UFO_FILTER(element);
    ufo_filter_process(self);
}

static void ufo_filter_finished(UfoElement *self)
{
    g_message("filter: received finished signal");
}

static void ufo_filter_dispose(GObject *object)
{
    UfoFilter *self = UFO_FILTER(object);
    UfoFilterPrivate *priv = UFO_FILTER_GET_PRIVATE(self);
#ifdef WITH_PROFILING
    g_message("Time for '%s'", priv->plugin_name);
    g_message("  GPU: %.4lfs", priv->gpu_time);
#endif

    /* Clears keys and unrefs queues */
    g_hash_table_destroy(priv->input_queues);
    g_hash_table_destroy(priv->output_queues);
    g_free(priv->plugin_name);
    G_OBJECT_CLASS(ufo_filter_parent_class)->dispose(object);
}


/*
 * Type/Class Initialization
 */
static void ufo_element_iface_init(UfoElementInterface *iface)
{
    /* virtual methods */
    iface->process = ufo_filter_iface_process;
    iface->set_output_queue = ufo_filter_set_output_queue;
    iface->set_command_queue = ufo_filter_set_command_queue;
    iface->get_input_queue = ufo_filter_get_input_queue;
    iface->get_output_queue = ufo_filter_get_output_queue;
    iface->get_command_queue = ufo_filter_get_command_queue;
    iface->get_time_spent = ufo_filter_get_time_spent;

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
    priv->command_queue = NULL;
    priv->cpu_time = 0.0f;
    priv->gpu_time = 0.0f;
    priv->input_queues = g_hash_table_new_full(g_str_hash, g_str_equal,
            g_free, (GDestroyNotify) g_async_queue_unref);
    priv->output_queues = g_hash_table_new_full(g_str_hash, g_str_equal,
            g_free, (GDestroyNotify) g_async_queue_unref);
}

