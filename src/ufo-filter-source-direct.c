/**
 * SECTION:ufo-filter-source-direct
 * @Short_description: A source filter provides data but does not consume any
 * @Title: UfoFilterSourceDirect
 *
 * A source filter produces data but does not accept any inputs. This can be
 * used to implement file readers or acquisition devices.
 */

#include <glib.h>
#include <gmodule.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "config.h"
#include "ufo-filter-source-direct.h"

G_DEFINE_TYPE (UfoFilterSourceDirect, ufo_filter_source_direct, UFO_TYPE_FILTER_SOURCE)

#define UFO_FILTER_SOURCE_DIRECT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_SOURCE_DIRECT, UfoFilterSourceDirectPrivate))

static UfoBuffer *END_OF_STREAM = GINT_TO_POINTER (1);

struct _UfoFilterSourceDirectPrivate {
    GAsyncQueue *incoming_queue;
    GAsyncQueue *outgoing_queue;
};

static void
ufo_filter_source_direct_initialize (UfoFilterSource *filter, guint **output_dim_sizes, GError **error)
{
    g_debug ("Virtual method `initialize' of %s not implemented",
             ufo_filter_get_plugin_name (UFO_FILTER (filter)));
}

static gboolean
ufo_filter_source_direct_generate (UfoFilterSource *filter, UfoBuffer *output[], GError **error)
{
    UfoFilterSourceDirectPrivate *priv;
    UfoBuffer *buffer;

    g_return_val_if_fail (UFO_IS_FILTER_SOURCE_DIRECT (filter), FALSE);

    priv = UFO_FILTER_SOURCE_DIRECT_GET_PRIVATE (filter);

    g_async_queue_push (priv->outgoing_queue, output[0]);
    buffer = g_async_queue_pop (priv->incoming_queue);

    return buffer == END_OF_STREAM;
}

void
ufo_filter_source_direct_push (UfoFilterSourceDirect *filter,
                               UfoBuffer *buffer)
{
    UfoFilterSourceDirectPrivate *priv;
    UfoBuffer *output;

    g_return_if_fail (UFO_IS_FILTER_SOURCE_DIRECT (filter));

    priv = UFO_FILTER_SOURCE_DIRECT_GET_PRIVATE (filter);
    output = g_async_queue_pop (priv->outgoing_queue);
    ufo_buffer_copy (buffer, output);
    g_async_queue_push (priv->incoming_queue, output);
}

void
ufo_filter_source_direct_stop (UfoFilterSourceDirect *filter)
{
    g_return_if_fail (UFO_IS_FILTER_SOURCE_DIRECT (filter));
    g_async_queue_push (filter->priv->incoming_queue, END_OF_STREAM);
}

static void
ufo_filter_source_direct_finalize (GObject *object)
{
    UfoFilterSourceDirectPrivate *priv;

    priv = UFO_FILTER_SOURCE_DIRECT_GET_PRIVATE (object);
    g_async_queue_unref (priv->outgoing_queue);
    g_async_queue_unref (priv->incoming_queue);
}

static void
ufo_filter_source_direct_class_init (UfoFilterSourceDirectClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);
    UfoFilterSourceClass *source_class = UFO_FILTER_SOURCE_CLASS (klass);

    source_class->initialize = ufo_filter_source_direct_initialize;
    source_class->generate = ufo_filter_source_direct_generate;
    oclass->finalize = ufo_filter_source_direct_finalize;

    g_type_class_add_private (klass, sizeof (UfoFilterSourceDirectPrivate));
}

static void
ufo_filter_source_direct_init (UfoFilterSourceDirect *self)
{
    UfoFilterSourceDirectPrivate *priv;
    UfoOutputParameter output_params[] = {{2}};

    self->priv = priv = UFO_FILTER_SOURCE_DIRECT_GET_PRIVATE (self);
    priv->incoming_queue = g_async_queue_new ();
    priv->outgoing_queue = g_async_queue_new ();

    ufo_filter_set_plugin_name (UFO_FILTER (self), "direct-source");
    ufo_filter_register_outputs (UFO_FILTER (self), 1, output_params);
}
