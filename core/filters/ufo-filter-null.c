#include <gmodule.h>
#include <CL/cl.h>

#include "ufo-resource-manager.h"
#include "ufo-filter-null.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"

struct _UfoFilterNullPrivate {
    /* add your private data here */
    float example;
};

GType ufo_filter_null_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(UfoFilterNull, ufo_filter_null, UFO_TYPE_FILTER);

#define UFO_FILTER_NULL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_NULL, UfoFilterNullPrivate))

static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

/* 
 * virtual methods 
 */
static void ufo_filter_null_initialize(UfoFilter *filter)
{
}

/*
 * This is the main method in which the filter processes one buffer after
 * another.
 */
static void ufo_filter_null_process(UfoFilter *filter)
{
    g_return_if_fail(UFO_IS_FILTER(filter));
    GAsyncQueue *input_queue = ufo_element_get_input_queue(UFO_ELEMENT(filter));
    UfoResourceManager *manager = ufo_resource_manager();

    UfoBuffer *input = (UfoBuffer *) g_async_queue_pop(input_queue);
    while (!ufo_buffer_is_finished(input)) {
        ufo_resource_manager_release_buffer(manager, input);
        input = (UfoBuffer *) g_async_queue_pop(input_queue);
    }
}

static void ufo_filter_null_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    /* Handle all properties accordingly */
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_null_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    /* Handle all properties accordingly */
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_null_class_init(UfoFilterNullClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);

    gobject_class->set_property = ufo_filter_null_set_property;
    gobject_class->get_property = ufo_filter_null_get_property;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;
    filter_class->initialize = ufo_filter_null_initialize;
    filter_class->process = ufo_filter_null_process;

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterNullPrivate));
}

static void ufo_filter_null_init(UfoFilterNull *self)
{
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_NULL, NULL);
}
