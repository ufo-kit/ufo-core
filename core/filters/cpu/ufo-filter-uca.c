#include <gmodule.h>
#include <uca/uca.h>
#include <uca/uca-cam.h>

#include "ufo-filter-uca.h"
#include "ufo-filter.h"
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

static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

/* 
 * virtual methods 
 */
static void ufo_filter_uca_dispose(GObject *object)
{
    UfoFilterUCAPrivate *priv = UFO_FILTER_UCA_GET_PRIVATE(object);
    priv->cam->stop_recording(priv->cam);
    uca_destroy(priv->u);

    G_OBJECT_CLASS(ufo_filter_uca_parent_class)->dispose(object);
}

static void ufo_filter_uca_process(UfoFilter *self)
{
    g_return_if_fail(UFO_IS_FILTER(self));

    UfoFilterUCAPrivate *priv = UFO_FILTER_UCA_GET_PRIVATE(self);
    struct uca_camera *cam = priv->cam;

    cam->start_recording(cam);
    UfoResourceManager *manager = ufo_filter_get_resource_manager(self);
    UfoBuffer *buffer = ufo_resource_manager_request_buffer(manager, 
        cam->frame_width, cam->frame_height);

    cam->grab(cam, (char *) ufo_buffer_get_cpu_data(buffer), NULL);
    ufo_buffer_reinterpret(buffer, UFO_BUFFER_DEPTH_8);

    g_message("send buffer %p", buffer);

    g_async_queue_push(ufo_filter_get_output_queue(self), buffer);
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
    self->priv->u = uca_init(NULL);
    self->priv->cam = self->priv->u->cameras;
    uca_cam_alloc(self->priv->cam, 10);
    g_message("uca instance = %p", self->priv->u);
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_UCA, NULL);
}
