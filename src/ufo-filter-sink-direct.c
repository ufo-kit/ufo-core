/**
 * SECTION:ufo-filter-sink-direct
 * @Short_description: A direct sink filter outputs data coming from a processing pipeline
 * @Title: UfoFilterSinkDirect
 */

#include <glib.h>
#include <gmodule.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "config.h"
#include "ufo-filter-sink-direct.h"

G_DEFINE_TYPE (UfoFilterSinkDirect, ufo_filter_sink_direct, UFO_TYPE_FILTER_SINK)

#define UFO_FILTER_SINK_DIRECT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_SINK_DIRECT, UfoFilterSinkDirectPrivate))

struct _UfoFilterSinkDirectPrivate {
    GAsyncQueue *outgoing_queue;
    GAsyncQueue *incoming_queue;
};

static void
ufo_filter_sink_direct_initialize (UfoFilterSink *filter, UfoBuffer *input[], GError **error)
{
    g_debug ("Virtual method `initialize' of %s not implemented",
             ufo_filter_get_plugin_name (UFO_FILTER (filter)));
}

static void
ufo_filter_sink_direct_consume (UfoFilterSink *filter, UfoBuffer *input[], GError **error)
{
    UfoFilterSinkDirectPrivate *priv;

    g_return_if_fail (UFO_IS_FILTER_SINK_DIRECT (filter));

    priv = UFO_FILTER_SINK_DIRECT_GET_PRIVATE (filter);
    g_async_queue_push (priv->outgoing_queue, input[0]);
    g_async_queue_pop (priv->incoming_queue);
}

/**
 * ufo_filter_sink_direct_pop:
 * @filter: A #UfoFilterSinkDirect object
 *
 * Get the buffer from this node. After processing the data, the buffer needs to
 * be released with ufo_filter_sink_direct_release().
 *
 * Returns: A #UfoBuffer to be processed directly.
 */
UfoBuffer *
ufo_filter_sink_direct_pop (UfoFilterSinkDirect *filter)
{
    g_return_val_if_fail (UFO_IS_FILTER_SINK_DIRECT (filter), NULL);
    return g_async_queue_pop (filter->priv->outgoing_queue);
}

/**
 * ufo_filter_sink_direct_release:
 * @filter: A #UfoFilterSinkDirect object
 * @buffer: A #UfoBuffer acquired with ufo_filter_sink_direct_pop().
 *
 * Release a buffer acquired with ufo_filter_sink_direct_pop().
 *
 * Note: If the buffer is not released, the processing pipeline will dead-lock.
 */
void
ufo_filter_sink_direct_release (UfoFilterSinkDirect *filter, UfoBuffer *buffer)
{
    g_return_if_fail (UFO_IS_FILTER_SINK_DIRECT (filter));
    g_async_queue_push (filter->priv->incoming_queue, buffer);
}

static void
ufo_filter_sink_direct_finalize (GObject *object)
{
    UfoFilterSinkDirectPrivate *priv;

    priv = UFO_FILTER_SINK_DIRECT_GET_PRIVATE (object);
    g_async_queue_unref (priv->outgoing_queue);
    g_async_queue_unref (priv->incoming_queue);
}

static void
ufo_filter_sink_direct_class_init (UfoFilterSinkDirectClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);
    UfoFilterSinkClass *filter_class = UFO_FILTER_SINK_CLASS (klass);

    filter_class->initialize = ufo_filter_sink_direct_initialize;
    filter_class->consume = ufo_filter_sink_direct_consume;
    oclass->finalize = ufo_filter_sink_direct_finalize;

    g_type_class_add_private (klass, sizeof (UfoFilterSinkDirectPrivate));
}

static void
ufo_filter_sink_direct_init (UfoFilterSinkDirect *self) {
    UfoFilterSinkDirectPrivate *priv;
    UfoInputParameter input_params[] = {{2, UFO_FILTER_INFINITE_INPUT}};

    self->priv = priv = UFO_FILTER_SINK_DIRECT_GET_PRIVATE (self);
    priv->outgoing_queue = g_async_queue_new ();
    priv->incoming_queue = g_async_queue_new ();

    ufo_filter_set_plugin_name (UFO_FILTER (self), "direct-sink");
    ufo_filter_register_inputs (UFO_FILTER (self), 1, input_params);
}
