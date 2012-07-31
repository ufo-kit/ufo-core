/**
 * SECTION:ufo-filter-reduce
 * @Short_description: A reduction filter accumulates one output from all inputs
 * @Title: UfoFilterReduce
 *
 * A reduction filter takes an arbitrary number of data input and produces one
 * output when the stream has finished. This scheme is useful for averaging the
 * data stream or producing a volume from a series of projections.
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

/**
 * ufo_filter_reduce_initialize:
 * @filter: A #UfoFilter.
 * @input: An array of buffers for each input port
 * @output_dims: The size of each dimension for each output
 * @default_value: The value to fill the output buffer
 * @error: Location for #GError.
 *
 * This function calls the implementation for the virtual initialize method. The
 * filter can use the input buffers as a hint to setup its own internal
 * structures. Moreover, it needs to return size of each output dimension in
 * each port as specified with ufo_filter_register_outputs():
 * <programlisting>
 * // register a 1-dimensional and a 2-dimensional output in object::init
 * ufo_filter_register_outputs (self, 1, 2, NULL);
 *
 * // specify sizes in object::initialize
 * output_dim_sizes[0][0] = 1024;
 *
 * output_dim_sizes[1][0] = 640;
 * output_dim_sizes[1][1] = 480;
 * </programlisting>
 *
 * It also has to set a valid default value with which the output buffer is
 * initialized.
 *
 * Since: 0.2
 */
void
ufo_filter_reduce_initialize (UfoFilterReduce *filter, UfoBuffer *input[], guint **output_dims, gfloat *default_value, GError **error)
{
    g_return_if_fail (UFO_IS_FILTER_REDUCE (filter));
    UFO_FILTER_REDUCE_GET_CLASS (filter)->initialize (filter, input, output_dims, default_value, error);
}

/**
 * ufo_filter_reduce_collect:
 * @filter: A #UfoFilter.
 * @input: An array of buffers for each input port
 * @output: An array of buffers for each output port
 * @cmd_queue: A %cl_command_queue object for ufo_buffer_get_host_array()
 * @error: Location for #GError.
 *
 * Process input data. The output buffer array contains the same buffers on each
 * method invocation and can be used to store accumulated values.
 *
 * Since: 0.2
 */
void
ufo_filter_reduce_collect (UfoFilterReduce *filter, UfoBuffer *input[], UfoBuffer *output[], gpointer cmd_queue, GError **error)
{
    g_return_if_fail (UFO_IS_FILTER_REDUCE (filter));
    UFO_FILTER_REDUCE_GET_CLASS (filter)->collect (filter, input, output, cmd_queue, error);
}

/**
 * ufo_filter_reduce_reduce:
 * @filter: A #UfoFilter.
 * @output: An array of buffers for each output port
 * @cmd_queue: A %cl_command_queue object for ufo_buffer_get_host_array()
 * @error: Location for #GError.
 *
 * This method calls the virtual reduce method and is called itself, when the
 * input data stream has finished. The reduce method can be used to finalize
 * work on the output buffers.
 *
 * Returns: TRUE if data is produced or FALSE if reduction has stopped
 *
 * Since: 0.2
 */
gboolean
ufo_filter_reduce_reduce (UfoFilterReduce *filter, UfoBuffer *output[], gpointer cmd_queue, GError **error)
{
    g_return_val_if_fail (UFO_IS_FILTER_REDUCE (filter), FALSE);
    return UFO_FILTER_REDUCE_GET_CLASS (filter)->reduce (filter, output, cmd_queue, error);
}

static void
ufo_filter_reduce_initialize_real (UfoFilterReduce *filter, UfoBuffer *input[], guint **output_dims, gfloat *default_value, GError **error)
{
    g_debug ("%s->initialize not implemented", ufo_filter_get_plugin_name (UFO_FILTER (filter)));
}

static void
ufo_filter_reduce_collect_real (UfoFilterReduce *filter, UfoBuffer *input[], UfoBuffer *output[], gpointer cmd_queue, GError **error)
{
    g_set_error (error, UFO_FILTER_ERROR, UFO_FILTER_ERROR_METHOD_NOT_IMPLEMENTED,
                 "Virtual method `collect' of %s is not implemented",
                 ufo_filter_get_plugin_name (UFO_FILTER (filter)));
}

static gboolean
ufo_filter_reduce_reduce_real (UfoFilterReduce *filter, UfoBuffer *output[], gpointer cmd_queue, GError **error)
{
    g_set_error (error, UFO_FILTER_ERROR, UFO_FILTER_ERROR_METHOD_NOT_IMPLEMENTED,
                 "Virtual method `reduce' of %s is not implemented",
                 ufo_filter_get_plugin_name (UFO_FILTER (filter)));

    return FALSE;
}

static void
ufo_filter_reduce_class_init (UfoFilterReduceClass *klass)
{
    klass->initialize = ufo_filter_reduce_initialize_real;
    klass->collect = ufo_filter_reduce_collect_real;
    klass->reduce = ufo_filter_reduce_reduce_real;
}

static void
ufo_filter_reduce_init (UfoFilterReduce *self)
{
    self->priv = UFO_FILTER_REDUCE_GET_PRIVATE (self);
}
