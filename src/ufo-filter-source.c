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

void
ufo_filter_source_initialize (UfoFilterSource *filter, guint **output_dim_sizes, GError **error)
{
    g_return_if_fail (UFO_IS_FILTER_SOURCE (filter));
    UFO_FILTER_SOURCE_GET_CLASS (filter)->initialize (filter, output_dim_sizes, error);
}

gboolean
ufo_filter_source_generate (UfoFilterSource *filter, UfoBuffer *results[], gpointer cmd_queue, GError **error)
{
    g_return_val_if_fail (UFO_IS_FILTER_SOURCE (filter), FALSE);
    return UFO_FILTER_SOURCE_GET_CLASS (filter)->generate (filter, results, cmd_queue, error);
}

static void
ufo_filter_source_initialize_real (UfoFilterSource *filter, guint **output_dim_sizes, GError **error)
{
    g_debug ("%s->initialize not implemented", ufo_filter_get_plugin_name (UFO_FILTER (filter)));
}

static gboolean
ufo_filter_source_generate_real (UfoFilterSource *filter, UfoBuffer *results[], gpointer cmd_queue, GError **error)
{
    g_set_error (error, UFO_FILTER_ERROR, UFO_FILTER_ERROR_METHOD_NOT_IMPLEMENTED,
            "Virtual method `generate` of %s is not implemented",
            ufo_filter_get_plugin_name (UFO_FILTER (filter)));

    return FALSE;
}

static void
ufo_filter_source_class_init(UfoFilterSourceClass *klass)
{
    klass->initialize = ufo_filter_source_initialize_real;
    klass->generate = ufo_filter_source_generate_real;
}

static void
ufo_filter_source_init(UfoFilterSource *self)
{
    self->priv = UFO_FILTER_SOURCE_GET_PRIVATE (self);
}
