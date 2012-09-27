/**
 * SECTION:ufo-profiler
 * @Short_description: Profile OpenCL kernel calls
 * @Title: UfoProfiler
 *
 * The #UfoProfiler provides a drop-in replacement for a manual
 * clEnqueueNDRangeKernel() call and tracks any associated events. The amount of
 * profiling can be controlled with an #UfoProfilerLevel when constructing the
 * profiler.
 *
 * Each #UfoFilter is assigned a profiler with ufo_profiler_set_profiler() by
 * the managing #UfoBaseScheduler. Filter implementations should call
 * ufo_filter_get_profiler() to receive their profiler and make profiled kernel
 * calls with ufo_profiler_call().
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
#include "ufo-aux.h"


G_DEFINE_TYPE(UfoProfiler, ufo_profiler, G_TYPE_OBJECT)

#define UFO_PROFILER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_PROFILER, UfoProfilerPrivate))

struct EventRow {
    cl_event    event;
    cl_kernel   kernel;
};

struct _UfoProfilerPrivate {
    UfoProfilerLevel level;

    GArray  *event_array;
    GTimer **timers;
};

enum {
    PROP_0,
    N_PROPERTIES
};

/**
 * timer_level:
 *
 * Maps UfoProfilerTimer to a UfoProfilerLevel
 */
static UfoProfilerLevel timer_level[] = {
    UFO_PROFILER_LEVEL_IO,      /* TIMER_IO */
    UFO_PROFILER_LEVEL_CPU,     /* TIMER_CPU */
    UFO_PROFILER_LEVEL_SYNC,    /* TIMER_FETCH */
    UFO_PROFILER_LEVEL_SYNC     /* TIMER_RELEASE */
};

/**
 * UfoProfilerLevel:
 * @UFO_PROFILER_LEVEL_NONE: Do not track any profiling information
 * @UFO_PROFILER_LEVEL_IO: Track I/O events
 * @UFO_PROFILER_LEVEL_OPENCL: Track OpenCL events
 * @UFO_PROFILER_LEVEL_SYNC: Track synchronization wait time
 *
 * Profiling levels that the profiler supports. To set the global profiling
 * level use the #UfoConfiguration:profile-level: property on a
 * #UfoConfiguration object set to the #UfoScheduler.
 */

/**
 * UfoProfilerTimer:
 * @UFO_PROFILER_TIMER_IO: Select I/O timer
 * @UFO_PROFILER_TIMER_CPU: Select CPU timer
 * @UFO_PROFILER_TIMER_LAST: Auxiliary value, do not use.
 *
 * Use these values to select a specific timer when calling
 * ufo_profiler_start(), ufo_profiler_stop() and ufo_profiler_elapsed().
 */

/**
 * ufo_profiler_new:
 * @level: Amount of information that should be tracked by the profiler.
 *
 * Create a profiler object.
 *
 * Return value: A new profiler object.
 */
UfoProfiler *
ufo_profiler_new (UfoProfilerLevel level)
{
    UfoProfiler *profiler;

    profiler = UFO_PROFILER (g_object_new (UFO_TYPE_PROFILER, NULL));
    profiler->priv->level = level;
    return profiler;
}

/**
 * ufo_profiler_call:
 * @profiler: A #UfoProfiler object.
 * @command_queue: An OpenCL command queue.
 * @kernel: An OpenCL kernel.
 * @work_dim: Number of working dimensions.
 * @global_work_size: Sizes of global dimensions. The array must have at least
 *      @work_dim entries.
 * @local_work_size: Sizes of local work group dimensions. The array must have
 *      at least @work_dim entries.
 *
 * Execute the @kernel using the command queue and execution parameters. The
 * event associated with the clEnqueueNDRangeKernel() call is recorded and may
 * be used for profiling purposes later on.
 */
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
    cl_event           *event_loc;
    cl_int              cl_err;
    struct EventRow     row;

    priv = profiler->priv;
    event_loc = priv->level & UFO_PROFILER_LEVEL_OPENCL ? &event : NULL;

    cl_err = clEnqueueNDRangeKernel (command_queue,
                                     kernel,
                                     work_dim,
                                     NULL,
                                     global_work_size,
                                     local_work_size,
                                     0, NULL, event_loc);
    CHECK_OPENCL_ERROR (cl_err);

    if (priv->level & UFO_PROFILER_LEVEL_OPENCL) {
        row.event = event;
        row.kernel = kernel;
        g_array_append_val (priv->event_array, row);
    }
}

void
ufo_profiler_start (UfoProfiler      *profiler,
                    UfoProfilerTimer  timer)
{
    g_return_if_fail (UFO_IS_PROFILER (profiler));

    if (profiler->priv->level & timer_level[timer])
        g_timer_continue (profiler->priv->timers[timer]);
}

void
ufo_profiler_stop (UfoProfiler       *profiler,
                   UfoProfilerTimer   timer)
{
    g_return_if_fail (UFO_IS_PROFILER (profiler));

    if (profiler->priv->level & timer_level[timer])
        g_timer_stop (profiler->priv->timers[timer]);
}

gdouble
ufo_profiler_elapsed (UfoProfiler       *profiler,
                      UfoProfilerTimer   timer)
{
    g_return_val_if_fail (UFO_IS_PROFILER (profiler), 0.0);
    return g_timer_elapsed (profiler->priv->timers[timer], NULL);
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

/**
 * ufo_profiler_foreach:
 * @profiler: A #UfoProfiler object.
 * @func: (scope call): The function to be called for an entry
 * @user_data: User parameters
 *
 * Iterates through the recorded events and calls @func for each entry.
 */
void
ufo_profiler_foreach (UfoProfiler    *profiler,
                      UfoProfilerFunc func,
                      gpointer        user_data)
{
    UfoProfilerPrivate *priv;
    struct EventRow *row;

    g_return_if_fail (UFO_IS_PROFILER (profiler));

    priv = profiler->priv;

    if (priv->level == UFO_PROFILER_LEVEL_NONE)
        return;

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

    for (guint i = 0; i < UFO_PROFILER_TIMER_LAST; i++)
        g_timer_destroy (priv->timers[i]);

    g_free (priv->timers);

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

    /* Setup timers for all events */
    priv->timers = g_new0 (GTimer *, UFO_PROFILER_TIMER_LAST);

    for (guint i = 0; i < UFO_PROFILER_TIMER_LAST; i++) {
        GTimer *timer;

        timer = g_timer_new ();
        g_timer_stop (timer);
        g_timer_reset (timer);
        priv->timers[i] = timer;
    }

    priv->level = UFO_PROFILER_LEVEL_NONE;
}
