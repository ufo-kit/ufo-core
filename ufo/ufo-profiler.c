/*
 * Copyright (C) 2011-2013 Karlsruhe Institute of Technology
 *
 * This file is part of Ufo.
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include <gmodule.h>
#include <glob.h>
#include <stdio.h>

#include <ufo/ufo-profiler.h>
#include <ufo/ufo-resources.h>

/**
 * SECTION:ufo-profiler
 * @Short_description: Profile different measures
 * @Title: UfoProfiler
 *
 * The #UfoProfiler provides a drop-in replacement for a manual
 * clEnqueueNDRangeKernel() call and tracks any associated events.
 *
 * Each #UfoFilter is assigned a profiler with ufo_profiler_set_profiler() by
 * the managing #UfoBaseScheduler. Filter implementations should call
 * ufo_filter_get_profiler() to receive their profiler and make profiled kernel
 * calls with ufo_profiler_call().
 *
 * Moreover, a profiler object is used to measure wall clock time for I/O,
 * synchronization and general CPU computation.
 */

G_DEFINE_TYPE(UfoProfiler, ufo_profiler, G_TYPE_OBJECT)

#define UFO_PROFILER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_PROFILER, UfoProfilerPrivate))

struct EventRow {
    cl_event    event;
    cl_kernel   kernel;
    cl_command_queue queue;
};

struct _UfoProfilerPrivate {
    GArray  *event_array;
    GTimer **timers;
    GList   *trace_events;
    gboolean trace;
};

enum {
    PROP_0,
    N_PROPERTIES
};

static GTimer *global_clock = NULL;

/**
 * UfoProfilerTimer:
 * @UFO_PROFILER_TIMER_IO: Select I/O timer
 * @UFO_PROFILER_TIMER_CPU: Select CPU timer
 * @UFO_PROFILER_TIMER_GPU: Select GPU timer
 * @UFO_PROFILER_TIMER_FETCH: Select timer that measures the synchronization
 *  time to fetch data from the queues.
 * @UFO_PROFILER_TIMER_RELEASE: Select timer that measures the synchronization
 *  time to push data to the queues.
 * @UFO_PROFILER_TIMER_LAST: Auxiliary value, do not use.
 *
 * Use these values to select a specific timer when calling
 * ufo_profiler_start(), ufo_profiler_stop() and ufo_profiler_elapsed().
 */

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

/**
 * ufo_profiler_call:
 * @profiler: A #UfoProfiler object.
 * @command_queue: A %cl_command_queue
 * @kernel: A %cl_kernel
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
    cl_int              cl_err;
    struct EventRow     row;

    g_return_if_fail (UFO_IS_PROFILER (profiler));
    priv = profiler->priv;

    cl_err = clEnqueueNDRangeKernel (command_queue,
                                     kernel,
                                     work_dim,
                                     NULL,
                                     global_work_size,
                                     local_work_size,
                                     0, NULL, &event);
    UFO_RESOURCES_CHECK_CLERR (cl_err);

    row.event = event;
    row.kernel = kernel;
    row.queue = command_queue;
    g_array_append_val (priv->event_array, row);
}

/**
 * ufo_profiler_start:
 * @profiler: A #UfoProfiler object.
 * @timer: Which timer to start
 *
 * Start @timer. The timer is not reset but accumulates the time elapsed between
 * ufo_profiler_start() and ufo_profiler_stop() calls.
 */
void
ufo_profiler_start (UfoProfiler      *profiler,
                    UfoProfilerTimer  timer)
{
    g_return_if_fail (UFO_IS_PROFILER (profiler));
    g_timer_continue (profiler->priv->timers[timer]);
}

/**
 * ufo_profiler_stop:
 * @profiler: A #UfoProfiler object.
 * @timer: Which timer to stop
 *
 * Stop @timer. The timer is not reset but accumulates the time elapsed between
 * ufo_profiler_start() and ufo_profiler_stop() calls.
 */
void
ufo_profiler_stop (UfoProfiler       *profiler,
                   UfoProfilerTimer   timer)
{
    g_return_if_fail (UFO_IS_PROFILER (profiler));
    g_timer_stop (profiler->priv->timers[timer]);
}

void
ufo_profiler_trace_event (UfoProfiler *profiler,
                          const gchar *name,
                          const gchar *type)
{
    UfoTraceEvent *event;
    gulong timestamp;

    g_return_if_fail (UFO_IS_PROFILER (profiler));
    if (!profiler->priv->trace)
        return;

    g_timer_elapsed (global_clock, &timestamp);

    event = g_malloc0 (sizeof(UfoTraceEvent));
    event->name = name;
    event->type = type;
    event->thread_id = g_thread_self ();
    event->timestamp = (gdouble) timestamp;
    profiler->priv->trace_events = g_list_append (profiler->priv->trace_events, event);
}


void
ufo_profiler_enable_tracing (UfoProfiler *profiler,
                             gboolean enable)
{
    g_return_if_fail (UFO_IS_PROFILER (profiler));
    profiler->priv->trace = enable;
}

/**
 * ufo_profiler_get_trace_events: (skip)
 * @profiler: A #UfoProfiler object.
 *
 * Get all events recorded with @profiler.
 *
 * Returns: (element-type UfoTraceEvent): A list with #UfoTraceEvent objects.
 */
GList *
ufo_profiler_get_trace_events (UfoProfiler *profiler)
{
    g_return_val_if_fail (UFO_IS_PROFILER (profiler), NULL);
    return profiler->priv->trace_events;
}

static gint
compare_event (const UfoTraceEvent *a,
               const UfoTraceEvent *b,
               gpointer user_data)
{
    return (gint) (a->timestamp - b->timestamp);
}

GList *
ufo_profiler_get_trace_events_sorted (UfoProfiler *profiler)
{
    GList *events = g_list_copy (ufo_profiler_get_trace_events (profiler));
    GList *sorted_events = g_list_sort (events, (GCompareFunc) compare_event);

    /* set relative timestamps based on the occurence of the first event */
    GList *first = g_list_first (sorted_events);
    gdouble base = ((UfoTraceEvent *) first->data)->timestamp;
    for (GList *it = g_list_first (sorted_events); it != NULL; it = g_list_next (it)) {
        UfoTraceEvent *event = (UfoTraceEvent *) it->data;
        event->timestamp_relative = event->timestamp - base;
    }

    return sorted_events;
}

static void
get_time_stamps (cl_event event, gulong *queued, gulong *submitted, gulong *start, gulong *end)
{
    UFO_RESOURCES_CHECK_CLERR (clWaitForEvents (1, &event));
    UFO_RESOURCES_CHECK_CLERR (clGetEventProfilingInfo (event, CL_PROFILING_COMMAND_QUEUED, sizeof (cl_ulong), queued, NULL));
    UFO_RESOURCES_CHECK_CLERR (clGetEventProfilingInfo (event, CL_PROFILING_COMMAND_SUBMIT, sizeof (cl_ulong), submitted, NULL));
    UFO_RESOURCES_CHECK_CLERR (clGetEventProfilingInfo (event, CL_PROFILING_COMMAND_START, sizeof (cl_ulong), start, NULL));
    UFO_RESOURCES_CHECK_CLERR (clGetEventProfilingInfo (event, CL_PROFILING_COMMAND_END, sizeof (cl_ulong), end, NULL));
}

static gdouble
gpu_elapsed (UfoProfilerPrivate *priv)
{
    struct EventRow *row;
    gdouble elapsed = 0.0;
    guint len = priv->event_array->len;

    if (len == 0)
        return 0.0;

    for (guint i = 0; i < len; i++) {
        gulong start, end;

        row = &g_array_index (priv->event_array, struct EventRow, i);

        UFO_RESOURCES_CHECK_CLERR (clGetEventInfo (row->event, CL_EVENT_COMMAND_QUEUE,
                                                   sizeof (cl_command_queue), &row->queue,
                                                   NULL));

        get_time_stamps (row->event, NULL, NULL, &start, &end);

        if (end < start)
            elapsed += (gdouble) ((G_MAXULONG - start) + end) * 1e-9;
        else
            elapsed += ((gdouble) (end - start)) * 1e-9;
    }

    return elapsed;
}

/**
 * ufo_profiler_elapsed:
 * @profiler: A #UfoProfiler object.
 * @timer: Which timer to start
 *
 * Get the elapsed time in seconds for @timer.
 *
 * Returns: Elapsed time in seconds.
 */
gdouble
ufo_profiler_elapsed (UfoProfiler       *profiler,
                      UfoProfilerTimer   timer)
{
    g_return_val_if_fail (UFO_IS_PROFILER (profiler), 0.0);

    if (timer == UFO_PROFILER_TIMER_GPU)
        return gpu_elapsed (profiler->priv);

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

void ufo_profiler_write_events_csv (UfoProfiler *profiler,
                                    gchar *filename)
{
    UfoProfilerPrivate *priv = UFO_PROFILER_GET_PRIVATE (profiler);
    FILE *fp = fopen (filename, "w");

    GList *events = ufo_profiler_get_trace_events_sorted (profiler);
   
    for (GList *it = g_list_first (events); it != NULL; it = g_list_next (it)) {
        UfoTraceEvent *event = (UfoTraceEvent *) it->data;
        g_debug ("[%f] %s %s", event->timestamp, event->name, event->type);
        fprintf (fp, "%.2f\t%.2f\t%s\t%s\n", event->timestamp, event->timestamp_relative,
                                    event->name, event->type);
    }
    fclose (fp);
}

static void
ufo_profiler_dispose (GObject *object)
{
    G_OBJECT_CLASS (ufo_profiler_parent_class)->dispose (object);
}

static void
ufo_profiler_finalize (GObject *object)
{
    UfoProfilerPrivate *priv;
    struct EventRow *row;

    G_OBJECT_CLASS (ufo_profiler_parent_class)->finalize (object);
    priv = UFO_PROFILER_GET_PRIVATE (object);

    for (guint i = 0; i < priv->event_array->len; i++) {
        row = &g_array_index (priv->event_array, struct EventRow, i);
        clReleaseEvent (row->event);
    }

    g_array_free (priv->event_array, TRUE);

    g_list_foreach (priv->trace_events, (GFunc) g_free, NULL);
    g_list_free (priv->trace_events);

    for (guint i = 0; i < UFO_PROFILER_TIMER_LAST; i++)
        g_timer_destroy (priv->timers[i]);

    g_free (priv->timers);
}

static void
ufo_profiler_class_init (UfoProfilerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->dispose = ufo_profiler_dispose;
    gobject_class->finalize = ufo_profiler_finalize;

    g_type_class_add_private (klass, sizeof (UfoProfilerPrivate));

    if (global_clock == NULL)
        global_clock = g_timer_new ();
}

static void
ufo_profiler_init (UfoProfiler *manager)
{
    UfoProfilerPrivate *priv;

    manager->priv = priv = UFO_PROFILER_GET_PRIVATE (manager);
    priv->event_array = g_array_sized_new (FALSE, TRUE, sizeof(struct EventRow), 2048);
    priv->trace_events = NULL;
    priv->trace = FALSE;

    /* Setup timers for all events */
    priv->timers = g_new0 (GTimer *, UFO_PROFILER_TIMER_LAST);

    for (guint i = 0; i < UFO_PROFILER_TIMER_LAST; i++) {
        GTimer *timer;

        timer = g_timer_new ();
        g_timer_stop (timer);
        g_timer_reset (timer);
        priv->timers[i] = timer;
    }
}
