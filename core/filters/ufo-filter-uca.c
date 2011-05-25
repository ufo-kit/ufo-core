#include <gmodule.h>
#include <uca/uca.h>

#include "ufo-filter-uca.h"
#include "ufo-filter.h"
#include "ufo-element.h"
#include "ufo-buffer.h"
#include "ufo-resource-manager.h"


struct _UfoFilterUCAPrivate {
    struct uca *u;
    struct uca_camera *cam;
};

GType ufo_filter_uca_get_type(void) G_GNUC_CONST;

/* Inherit from UFO_TYPE_FILTER */
G_DEFINE_TYPE(UfoFilterUCA, ufo_filter_uca, UFO_TYPE_FILTER);

#define UFO_FILTER_UCA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_UCA, UfoFilterUCAPrivate))


/* 
 * virtual methods 
 */
static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

static void ufo_filter_uca_dispose(GObject *object)
{
    UfoFilterUCAPrivate *priv = UFO_FILTER_UCA_GET_PRIVATE(object);
    uca_cam_stop_recording(priv->cam);
    uca_destroy(priv->u);

    G_OBJECT_CLASS(ufo_filter_uca_parent_class)->dispose(object);
}

static void ufo_filter_uca_process(UfoFilter *self)
{
    g_return_if_fail(UFO_IS_FILTER(self));

    UfoFilterUCAPrivate *priv = UFO_FILTER_UCA_GET_PRIVATE(self);
    struct uca_camera *cam = priv->cam;

    uint32_t width, height;
    uca_cam_get_property(cam, UCA_PROP_WIDTH, &width, 0);
    uca_cam_get_property(cam, UCA_PROP_HEIGHT, &height, 0);

    uca_cam_start_recording(cam);
    UfoResourceManager *manager = ufo_resource_manager();
    GAsyncQueue *output_queue = ufo_element_get_output_queue(UFO_ELEMENT(self));
    cl_command_queue command_queue = (cl_command_queue) ufo_element_get_command_queue(UFO_ELEMENT(self));

    for (guint i = 0; i < 1; i++) {
        UfoBuffer *buffer = ufo_resource_manager_request_buffer(manager, 
                width, height, NULL);

        uca_cam_grab(cam, (char *) ufo_buffer_get_cpu_data(buffer, command_queue), NULL);
        /* FIXME: don't use hardcoded 8 bits per pixel */
        ufo_buffer_reinterpret(buffer, 8, width * height);

        g_message("[uca] send buffer %p to queue %p", buffer, output_queue);
        g_async_queue_push(output_queue, buffer);
    }

    /* No more data */
    UfoBuffer *buffer = ufo_resource_manager_request_buffer(manager, 1, 1, NULL);
    g_object_set(buffer,
            "finished", TRUE,
            NULL);
    g_async_queue_push(output_queue, buffer);
}

static void ufo_filter_uca_class_init(UfoFilterUCAClass *klass)
{
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = ufo_filter_uca_dispose;
    filter_class->process = ufo_filter_uca_process;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;

    /* install private data */
    g_type_class_add_private(object_class, sizeof(UfoFilterUCAPrivate));
}

static void ufo_filter_uca_init(UfoFilterUCA *self)
{
    /* init public fields */

    /* init private fields */
    self->priv = UFO_FILTER_UCA_GET_PRIVATE(self);
    /* FIXME: what to do when u == NULL? */
    self->priv->u = uca_init(NULL);
    self->priv->cam = self->priv->u->cameras;
    uca_cam_alloc(self->priv->cam, 10);
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_UCA, NULL);
}
