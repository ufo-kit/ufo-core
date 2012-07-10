/**
 * SECTION:ufo-filter
 * @Short_description: Single unit of computation
 * @Title: UfoFilterReduce
 */

#include <glib.h>
#include <gmodule.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "config.h"
#include "ufo-filter-reduce.h"

G_DEFINE_TYPE (UfoFilterReduce, ufo_filter_reduce, UFO_TYPE_FILTER)

#define UFO_FILTER_REDUCE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_REDUCE, UfoFilterReducePrivate))

void
ufo_filter_reduce_initialize (UfoFilterReduce *filter, UfoBuffer *input[], guint **output_dims, GError **error)
{
    g_return_if_fail (UFO_IS_FILTER_REDUCE (filter));
    UFO_FILTER_REDUCE_GET_CLASS (filter)->initialize (filter, input, output_dims, error);
}

void
ufo_filter_reduce_collect (UfoFilterReduce  *filter, UfoBuffer *input[], gpointer cmd_queue, GError **error)
{
    g_return_if_fail (UFO_IS_FILTER_REDUCE (filter));
    UFO_FILTER_REDUCE_GET_CLASS (filter)->collect (filter, input, cmd_queue, error);
}

void
ufo_filter_reduce_reduce (UfoFilterReduce  *filter, UfoBuffer *output[], gpointer cmd_queue, GError **error)
{
    g_return_if_fail (UFO_IS_FILTER_REDUCE (filter));
    UFO_FILTER_REDUCE_GET_CLASS (filter)->reduce (filter, output, cmd_queue, error);
}

static void
ufo_filter_reduce_initialize_real (UfoFilterReduce *filter, UfoBuffer *input[], guint **output_dims, GError **error)
{
    g_debug ("%s->initialize not implemented", ufo_filter_get_plugin_name (UFO_FILTER (filter)));
}

static void
ufo_filter_reduce_collect_real (UfoFilterReduce *filter, UfoBuffer *input[], gpointer cmd_queue, GError **error)
{
    g_set_error (error, UFO_FILTER_ERROR, UFO_FILTER_ERROR_METHOD_NOT_IMPLEMENTED,
            "Virtual method `collect` of %s is not implemented",
            ufo_filter_get_plugin_name (UFO_FILTER (filter)));
}

static void
ufo_filter_reduce_reduce_real (UfoFilterReduce *filter, UfoBuffer *output[], gpointer cmd_queue, GError **error)
{
    g_set_error (error, UFO_FILTER_ERROR, UFO_FILTER_ERROR_METHOD_NOT_IMPLEMENTED,
            "Virtual method `collect` of %s is not implemented",
            ufo_filter_get_plugin_name (UFO_FILTER (filter)));
}

static void
ufo_filter_reduce_class_init(UfoFilterReduceClass *klass)
{
    klass->initialize = ufo_filter_reduce_initialize_real;
    klass->collect = ufo_filter_reduce_collect_real;
    klass->reduce = ufo_filter_reduce_reduce_real;
}

static void
ufo_filter_reduce_init(UfoFilterReduce *self)
{
    self->priv = UFO_FILTER_REDUCE_GET_PRIVATE (self);
}
