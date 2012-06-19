/**
 * SECTION:ufo-filter
 * @Short_description: Single unit of computation
 * @Title: UfoFilterSource
 */

#include <glib.h>
#include <gmodule.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "config.h"
#include "ufo-filter-source.h"

G_DEFINE_TYPE (UfoFilterSource, ufo_filter_source, UFO_TYPE_FILTER)

#define UFO_FILTER_SOURCE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_SOURCE, UfoFilterSourcePrivate))

struct _UfoFilterSourcePrivate {
    gboolean            finished;
};

GError *
ufo_filter_source_initialize (UfoFilterSource *filter, guint **output_dim_sizes)
{
    g_return_val_if_fail (UFO_IS_FILTER_SOURCE (filter), NULL);
    return UFO_FILTER_SOURCE_GET_CLASS (filter)->source_initialize (filter, output_dim_sizes);
}

gboolean
ufo_filter_source_generate (UfoFilterSource *filter, UfoBuffer *results[], gpointer cmd_queue, GError **error)
{
    g_return_val_if_fail (UFO_IS_FILTER_SOURCE (filter), FALSE);
    return UFO_FILTER_SOURCE_GET_CLASS (filter)->generate (filter, results, cmd_queue, error);
}

static void
ufo_filter_source_finalize(GObject *object)
{
    G_OBJECT_CLASS (ufo_filter_source_parent_class)->finalize (object);
}

static void
ufo_filter_source_class_init(UfoFilterSourceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = ufo_filter_source_finalize;

    klass->source_initialize = NULL;
    klass->generate = NULL;

    g_type_class_add_private (klass, sizeof(UfoFilterSourcePrivate));
}

static void
ufo_filter_source_init(UfoFilterSource *self)
{
    self->priv = UFO_FILTER_SOURCE_GET_PRIVATE (self);
}
