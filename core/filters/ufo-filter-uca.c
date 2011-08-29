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
    gint count;
    double time;
};

GType ufo_filter_uca_get_type(void) G_GNUC_CONST;

/* Inherit from UFO_TYPE_FILTER */
G_DEFINE_TYPE(UfoFilterUCA, ufo_filter_uca, UFO_TYPE_FILTER);

#define UFO_FILTER_UCA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_UCA, UfoFilterUCAPrivate))

enum {
    PROP_0,
    PROP_COUNT,
    PROP_TIME,
    N_PROPERTIES
};

static GParamSpec *uca_properties[N_PROPERTIES] = { NULL, };

/* 
 * virtual methods 
 */
static void activated(EthosPlugin *plugin)
{
}

static void deactivated(EthosPlugin *plugin)
{
}

static void ufo_filter_uca_set_property(GObject *object,
    guint           property_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
    UfoFilterUCA *filter = UFO_FILTER_UCA(object);

    switch (property_id) {
        case PROP_COUNT:
            filter->priv->count = g_value_get_int(value);
            break;
        case PROP_TIME:
            filter->priv->time = g_value_get_double(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_uca_get_property(GObject *object,
    guint       property_id,
    GValue      *value,
    GParamSpec  *pspec)
{
    UfoFilterUCA *filter = UFO_FILTER_UCA(object);

    switch (property_id) {
        case PROP_COUNT:
            g_value_set_int(value, filter->priv->count);
            break;
        case PROP_TIME:
            g_value_set_double(value, filter->priv->time);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_filter_uca_dispose(GObject *object)
{
    UfoFilterUCAPrivate *priv = UFO_FILTER_UCA_GET_PRIVATE(object);
    g_message("stop recording and camera");
    uca_cam_stop_recording(priv->cam);
    uca_destroy(priv->u);

    G_OBJECT_CLASS(ufo_filter_uca_parent_class)->dispose(object);
}

static void ufo_filter_uca_process(UfoFilter *self)
{
    g_return_if_fail(UFO_IS_FILTER(self));
    UfoFilterUCAPrivate *priv = UFO_FILTER_UCA_GET_PRIVATE(self);
    UfoResourceManager *manager = ufo_resource_manager();
    GAsyncQueue *output_queue = ufo_element_get_output_queue(UFO_ELEMENT(self));

    /* Camera subsystem could not be initialized, so flag end */
    if (priv->u == NULL) {
        g_debug("Camera system is not initialized");
        g_async_queue_push(output_queue, 
                ufo_resource_manager_request_finish_buffer(manager));
        return;
    }

    cl_command_queue command_queue = (cl_command_queue) ufo_element_get_command_queue(UFO_ELEMENT(self));
    struct uca_camera *cam = priv->cam;

    uint32_t width, height;
    uca_cam_get_property(cam, UCA_PROP_WIDTH, &width, 0);
    uca_cam_get_property(cam, UCA_PROP_HEIGHT, &height, 0);
    gint32 dimensions[4] = { width, height, 1, 1 };

    uca_cam_start_recording(cam);
    GTimer *timer = g_timer_new();

    for (guint i = 0; i < priv->count || g_timer_elapsed(timer, NULL) < priv->time; i++) {
        UfoBuffer *buffer = ufo_resource_manager_request_buffer(manager, 
                UFO_BUFFER_2D, dimensions, NULL, FALSE);

        uca_cam_grab(cam, (char *) ufo_buffer_get_cpu_data(buffer, command_queue), NULL);

        /* FIXME: don't use hardcoded 8 bits per pixel */
        ufo_buffer_reinterpret(buffer, 8, width * height);
        while (g_async_queue_length(output_queue) > 2)
            ;

        g_async_queue_push(output_queue, buffer);
    }

    g_timer_destroy(timer);
    g_async_queue_push(output_queue, 
            ufo_resource_manager_request_finish_buffer(manager));
}

static void ufo_filter_uca_class_init(UfoFilterUCAClass *klass)
{
    UfoFilterClass *filter_class = UFO_FILTER_CLASS(klass);
    EthosPluginClass *plugin_class = ETHOS_PLUGIN_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->set_property = ufo_filter_uca_set_property;
    gobject_class->get_property = ufo_filter_uca_get_property;
    gobject_class->dispose = ufo_filter_uca_dispose;
    filter_class->process = ufo_filter_uca_process;
    plugin_class->activated = activated;
    plugin_class->deactivated = deactivated;

    uca_properties[PROP_COUNT] =
        g_param_spec_int("count",
        "Number of frames to record",
        "Number of frames to record",
        0,     /* minimum */
        8192,   /* maximum */
        0,     /* default */
        G_PARAM_READWRITE);

    uca_properties[PROP_TIME] = g_param_spec_double("time",
        "Maximum time for recording in fraction of seconds",
        "Maximum time for recording in fraction of seconds",
         0.0,       /* minimum */
         3600.0,    /* maximum */
         5.0,       /* default */
        G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_COUNT, uca_properties[PROP_COUNT]);
    g_object_class_install_property(gobject_class, PROP_TIME, uca_properties[PROP_TIME]);

    /* install private data */
    g_type_class_add_private(gobject_class, sizeof(UfoFilterUCAPrivate));
}

static void ufo_filter_uca_init(UfoFilterUCA *self)
{
    /* init public fields */

    /* init private fields */
    self->priv = UFO_FILTER_UCA_GET_PRIVATE(self);
    self->priv->cam = NULL;
    /* FIXME: what to do when u == NULL? */
    self->priv->u = uca_init(NULL);
    if (self->priv->u == NULL)
        return;

    self->priv->cam = self->priv->u->cameras;
    self->priv->count = 0;
    uca_cam_alloc(self->priv->cam, 10);
}

G_MODULE_EXPORT EthosPlugin *ethos_plugin_register(void)
{
    return g_object_new(UFO_TYPE_FILTER_UCA, NULL);
}
