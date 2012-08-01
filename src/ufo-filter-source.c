/**
 * SECTION:ufo-filter-source
 * @Short_description: A source filter provides data but does not consume any
 * @Title: UfoFilterSource
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
#include "ufo-filter-source.h"

G_DEFINE_TYPE (UfoFilterSource, ufo_filter_source, UFO_TYPE_FILTER)

#define UFO_FILTER_SOURCE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_SOURCE, UfoFilterSourcePrivate))

/**
 * ufo_filter_source_initialize:
 * @filter: A #UfoFilter.
 * @output_dim_sizes: The size of each dimension for each output
 * @error: Location for #GError.
 *
 * This function calls the implementation for the virtual initialize method. It
 * needs to return size of each output dimension in each port as specified with
 * ufo_filter_register_outputs():
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
 * Since: 0.2
 */
void
ufo_filter_source_initialize (UfoFilterSource *filter, guint **output_dim_sizes, GError **error)
{
    g_return_if_fail (UFO_IS_FILTER_SOURCE (filter));
    UFO_FILTER_SOURCE_GET_CLASS (filter)->initialize (filter, output_dim_sizes, error);
}

/**
 * ufo_filter_source_generate:
 * @filter: A #UfoFilter.
 * @output: An array of buffers for each output port
 * @cmd_queue: A %cl_command_queue object for ufo_buffer_get_host_array()
 * @error: Location for #GError.
 *
 * This function calls the implementation for the virtual generate method. It
 * should produce one set of outputs for each time it is called. If no more data
 * is produced it must return %FALSE.
 *
 * Returns: %TRUE if data is produced, otherwise %FALSE.
 * Since: 0.2
 */
gboolean
ufo_filter_source_generate (UfoFilterSource *filter, UfoBuffer *output[], gpointer cmd_queue, GError **error)
{
    g_return_val_if_fail (UFO_IS_FILTER_SOURCE (filter), FALSE);
    return UFO_FILTER_SOURCE_GET_CLASS (filter)->generate (filter, output, cmd_queue, error);
}

static void
ufo_filter_source_initialize_real (UfoFilterSource *filter, guint **output_dim_sizes, GError **error)
{
    g_debug ("Virtual method `initialize' of %s not implemented",
             ufo_filter_get_plugin_name (UFO_FILTER (filter)));
}

static gboolean
ufo_filter_source_generate_real (UfoFilterSource *filter, UfoBuffer *output[], gpointer cmd_queue, GError **error)
{
    g_set_error (error, UFO_FILTER_ERROR, UFO_FILTER_ERROR_METHOD_NOT_IMPLEMENTED,
                 "Virtual method `generate' of %s is not implemented",
                 ufo_filter_get_plugin_name (UFO_FILTER (filter)));

    return FALSE;
}

static void
ufo_filter_source_class_init (UfoFilterSourceClass *klass)
{
    klass->initialize = ufo_filter_source_initialize_real;
    klass->generate = ufo_filter_source_generate_real;
}

static void
ufo_filter_source_init (UfoFilterSource *self)
{
}
