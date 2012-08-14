/**
 * SECTION:ufo-plugin-manager
 * @Short_description: Load an #UfoFilter from a shared object
 * @Title: UfoProfiler
 *
 * The plugin manager opens shared object modules searched for in locations
 * specified with ufo_profiler_add_paths(). An #UfoFilter can be
 * instantiated with ufo_profiler_get_filter() with a one-to-one mapping
 * between filter name xyz and module name libfilterxyz.so. Any errors are
 * reported as one of #UfoProfilerError codes.
 */

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include <gmodule.h>
#include <glob.h>
#include "config.h"
#include "ufo-profiler.h"


G_DEFINE_TYPE(UfoProfiler, ufo_profiler, G_TYPE_OBJECT)

#define UFO_PROFILER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_PROFILER, UfoProfilerPrivate))

struct EventRow {
    cl_event    event;
    cl_kernel   kernel;
};

struct _UfoProfilerPrivate {
    GArray *event_array;    /**< maps from UfoFilter* to GArray* */
};

enum {
    PROP_0,
    N_PROPERTIES
};


/**
 * ufo_profiler_new:
 *
 * Create a profiler object.
 *
 * Return value: A new profiler object.
 */
UfoProfiler *
ufo_profiler_new (void)
{
    return UFO_PROFILER (g_object_new (UFO_TYPE_PROFILER, NULL));
}

void
ufo_profiler_call (UfoProfiler    *profiler,
                   gpointer        command_queue,
                   gpointer        kernel,
                   guint           work_dim,
                   const gsize    *global_work_size,
                   const gsize    *local_work_size)
{
    UfoProfilerPrivate *priv;
    cl_event            event;
    cl_int              cl_err;
    struct EventRow     row;

    priv = profiler->priv;

    cl_err = clEnqueueNDRangeKernel (command_queue,
                                     kernel,
                                     work_dim,
                                     NULL,
                                     global_work_size,
                                     local_work_size,
                                     0, NULL, &event);
    CHECK_OPENCL_ERROR (cl_err);

    row.event = event;
    row.kernel = kernel;
    g_array_append_val (priv->event_array, row);
}

static gchar *
get_kernel_name (cl_kernel kernel)
{
    gsize size;
    gchar *s;

    clGetKernelInfo (kernel, CL_KERNEL_FUNCTION_NAME, 0, NULL, &size);
    s = g_malloc0(size + 1);
    clGetKernelInfo (kernel, CL_KERNEL_FUNCTION_NAME, size, s, NULL);
    return s;
}

static void
get_time_stamps (cl_event event, gulong *queued, gulong *submitted, gulong *start, gulong *end)
{
    clGetEventProfilingInfo (event, CL_PROFILING_COMMAND_QUEUED, sizeof (cl_ulong), queued, NULL);
    clGetEventProfilingInfo (event, CL_PROFILING_COMMAND_SUBMIT, sizeof (cl_ulong), submitted, NULL);
    clGetEventProfilingInfo (event, CL_PROFILING_COMMAND_START, sizeof (cl_ulong), start, NULL);
    clGetEventProfilingInfo (event, CL_PROFILING_COMMAND_END, sizeof (cl_ulong), end, NULL);
}

void
ufo_profiler_foreach (UfoProfiler    *profiler,
                      UfoProfilerFunc func,
                      gpointer        user_data)
{
    UfoProfilerPrivate *priv;
    struct EventRow *row;

    g_return_if_fail (UFO_IS_PROFILER (profiler));

    priv = profiler->priv;

    for (guint i = 0; i < priv->event_array->len; i++) {
        cl_command_queue queue;
        gulong queued, submitted, start, end;
        gchar *kernel_name;
        gchar *row_string;

        row = &g_array_index (priv->event_array, struct EventRow, i);

        kernel_name = get_kernel_name (row->kernel);
        clGetEventInfo (row->event, CL_EVENT_COMMAND_QUEUE, sizeof (cl_command_queue), &queue, NULL);
        get_time_stamps (row->event, &queued, &submitted, &start, &end);

        row_string = g_strdup_printf ("%s %p %lu %lu %lu %lu",
                                      kernel_name,
                                      (gpointer) queue,
                                      queued, submitted, start, end);
        func (row_string, user_data);

        g_free (row_string);
        g_free (kernel_name);
    }
}

static void
ufo_profiler_dispose (GObject *object)
{
    G_OBJECT_CLASS (ufo_profiler_parent_class)->dispose (object);
    g_message ("UfoProfiler: disposed");
}

static void
ufo_profiler_finalize (GObject *object)
{
    UfoProfilerPrivate *priv;

    G_OBJECT_CLASS (ufo_profiler_parent_class)->finalize (object);
    priv = UFO_PROFILER_GET_PRIVATE (object);

    g_array_free (priv->event_array, TRUE);

    g_message ("UfoProfiler: finalized");
}

static void
ufo_profiler_class_init (UfoProfilerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->dispose = ufo_profiler_dispose;
    gobject_class->finalize = ufo_profiler_finalize;

    g_type_class_add_private (klass, sizeof (UfoProfilerPrivate));
}

static void
ufo_profiler_init (UfoProfiler *manager)
{
    UfoProfilerPrivate *priv;

    manager->priv = priv = UFO_PROFILER_GET_PRIVATE (manager);
    priv->event_array = g_array_sized_new (FALSE, TRUE, sizeof(struct EventRow), 2048);
}
