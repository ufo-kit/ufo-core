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

#ifndef __UFO_PROFILER_H
#define __UFO_PROFILER_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_PROFILER             (ufo_profiler_get_type())
#define UFO_PROFILER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_PROFILER, UfoProfiler))
#define UFO_IS_PROFILER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_PROFILER))
#define UFO_PROFILER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_PROFILER, UfoProfilerClass))
#define UFO_IS_PROFILER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_PROFILER))
#define UFO_PROFILER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_PROFILER, UfoProfilerClass))

typedef struct _UfoProfiler           UfoProfiler;
typedef struct _UfoProfilerClass      UfoProfilerClass;
typedef struct _UfoProfilerPrivate    UfoProfilerPrivate;

/**
 * UfoProfiler:
 *
 * The #UfoProfiler collects and records OpenCL events and stores them in a
 * convenient format on disk or prints summaries on screen.
 */
struct _UfoProfiler {
    /*< private >*/
    GObject parent_instance;

    UfoProfilerPrivate *priv;
};

/**
 * UfoProfilerFunc:
 * @kernel: Kernel name
 * @queue: OpenCL command queue
 * @queued: Queuing timestamp in ns
 * @submitted: Submit timestamp in ns
 * @start: Start timestamp in ns
 * @end: End timestamp in ns
 * @user_data: User data passed to ufo_profiler_foreach().
 *
 * Specifies the type of functions passed to ufo_profiler_foreach().
 */
typedef void (*UfoProfilerFunc) (const gchar *kernel,
                                 gconstpointer queue,
                                 gulong queued,
                                 gulong submitted,
                                 gulong start,
                                 gulong end,
                                 gpointer user_data);

/**
 * UfoProfilerClass:
 *
 * #UfoProfiler class
 */
struct _UfoProfilerClass {
    /*< private >*/
    GObjectClass parent_class;
};

/**
 * UfoTraceEventType:
 * @UFO_TRACE_EVENT_PROCESS: A process event
 * @UFO_TRACE_EVENT_GENERATE: A generate event
 * @UFO_TRACE_EVENT_BEGIN: Beginning of an event
 * @UFO_TRACE_EVENT_END: End of an event
 */
typedef enum {
    UFO_TRACE_EVENT_PROCESS     = 1 << 0,
    UFO_TRACE_EVENT_GENERATE    = 1 << 1,
    UFO_TRACE_EVENT_BEGIN       = 1 << 2,
    UFO_TRACE_EVENT_END         = 1 << 3,
    UFO_TRACE_EVENT_NETWORK     = 1 << 4,
} UfoTraceEventType;

#define UFO_TRACE_EVENT_TYPE_MASK   (UFO_TRACE_EVENT_PROCESS | UFO_TRACE_EVENT_GENERATE)
#define UFO_TRACE_EVENT_TIME_MASK   (UFO_TRACE_EVENT_BEGIN | UFO_TRACE_EVENT_END)

/**
 * UfoTraceEvent:
 * @type: Type of the event
 * @thread_id: ID of thread in which the event was issued
 * @timestamp: Arbitrary timestamp of the event
 */
typedef struct {
    UfoTraceEventType type;
    gpointer     thread_id;
    gdouble      timestamp;
    gchar       *args;
} UfoTraceEvent;

typedef enum {
    UFO_PROFILER_TIMER_IO = 0,
    UFO_PROFILER_TIMER_CPU,
    UFO_PROFILER_TIMER_GPU,
    UFO_PROFILER_TIMER_FETCH,
    UFO_PROFILER_TIMER_RELEASE,
    UFO_PROFILER_TIMER_LAST
} UfoProfilerTimer;

UfoProfiler *ufo_profiler_new           (void);
void         ufo_profiler_call          (UfoProfiler        *profiler,
                                         gpointer            command_queue,
                                         gpointer            kernel,
                                         guint               work_dim,
                                         const gsize        *global_work_size,
                                         const gsize        *local_work_size);
void         ufo_profiler_register_event
                                        (UfoProfiler *profiler,
                                         gpointer command_queue,
                                         gpointer kernel,
                                         gpointer event);
void         ufo_profiler_foreach       (UfoProfiler        *profiler,
                                         UfoProfilerFunc     func,
                                         gpointer            user_data);
void         ufo_profiler_start         (UfoProfiler        *profiler,
                                         UfoProfilerTimer    timer);
void         ufo_profiler_stop          (UfoProfiler        *profiler,
                                         UfoProfilerTimer    timer);
void         ufo_profiler_trace_event   (UfoProfiler        *profiler,
                                         UfoTraceEventType   type,
                                         const gchar        *args);
void         ufo_profiler_enable_tracing
                                        (UfoProfiler        *profiler,
                                         gboolean            enable);
GList       *ufo_profiler_get_trace_events
                                        (UfoProfiler        *profiler);
gdouble      ufo_profiler_elapsed       (UfoProfiler        *profiler,
                                         UfoProfilerTimer    timer);
GType        ufo_profiler_get_type      (void);

G_END_DECLS

#endif
