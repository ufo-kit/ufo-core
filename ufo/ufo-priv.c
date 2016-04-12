#include <stdio.h>
#include "ufo-priv.h"
#include "ufo/compat.h"
#include "ufo/ufo-profiler.h"
#include "ufo/ufo-task-node.h"


typedef struct {
    const gchar *name;
    gchar *tid;
    gsize pid;
    gchar type;
    gdouble timestamp;
    gconstpointer data;
} Event;

typedef struct {
    GList *events;
} EventContainer;


static gint
compare_events (const Event *a,
                const Event *b)
{
    if (a->timestamp != b->timestamp)
        return a->timestamp < b->timestamp ? -1 : 1;

    /* Check that 'B' < 'E' */
    return (a->type - b->type);
}


static GList *
get_sorted_trace_events (GList *nodes)
{
    GList *it;
    GList *sorted = NULL;

    g_list_for (nodes, it) {
        UfoTaskNode *node;
        UfoProfiler *profiler;
        GList *events;
        GList *jt;

        node = UFO_TASK_NODE (it->data);
        profiler = ufo_task_node_get_profiler (node);
        events = ufo_profiler_get_trace_events (profiler);

        g_list_for (events, jt) {
            UfoTraceEvent *trace_event;
            Event *event;

            trace_event = (UfoTraceEvent *) jt->data;

            event = g_new0 (Event, 1);
            event->timestamp = trace_event->timestamp;

            if (trace_event->type & UFO_TRACE_EVENT_BEGIN)
                event->type = 'B';

            if (trace_event->type & UFO_TRACE_EVENT_END)
                event->type = 'E';

            if (trace_event->type & UFO_TRACE_EVENT_PROCESS)
                event->name = "process";

            if (trace_event->type & UFO_TRACE_EVENT_GENERATE)
                event->name = "generate";

            event->pid = 1;
            event->tid = g_strdup_printf ("%s-%p", G_OBJECT_TYPE_NAME (node), (gpointer) node);
            sorted = g_list_insert_sorted (sorted, event, (GCompareFunc) compare_events);
        }
    }

    return sorted;
}

static Event *
make_event (const gchar *kernel, gconstpointer queue, gchar type, gulong timestamp)
{
    Event *event;

    event = g_new0 (Event, 1);
    event->name = g_strdup (kernel);
    event->tid = g_strdup (kernel);
    event->pid = (gsize) queue;
    event->type = type;
    /* Convert from OpenCL ns to seconds in order to get µs in the trace view */
    event->timestamp = timestamp * 1.0e-9;

    return event;
}

static void
add_events (const gchar *kernel, gconstpointer queue,
            gulong queued, gulong submitted, gulong start, gulong end,
            EventContainer *c)
{
    c->events = g_list_append (c->events, make_event (kernel, queue, 'B', start));
    c->events = g_list_append (c->events, make_event (kernel, queue, 'E', end));
}

static void
write_trace_json (const gchar *filename_template, GList *events)
{
    FILE *fp;
    GList *it;
    GDateTime *now;
    gchar *filename;
    gchar *timestr;

    now = g_date_time_new_now_local ();
    timestr = g_date_time_format (now, "%FT%T%z");
    filename = g_strdup_printf (filename_template, timestr);

    fp = fopen (filename, "w");
    fprintf (fp, "{ \"traceEvents\": [");

    g_list_for (events, it) {
        Event *event = (Event *) it->data;
        gdouble timestamp = event->timestamp * 1000 * 1000;
        fprintf (fp, "{\"cat\":\"f\",\"ph\": \"%c\", \"ts\": %.0f, \"pid\": %zu, \"tid\": \"%s\",\"name\": \"%s\", \"args\": {}}",
                     event->type, timestamp, event->pid, event->tid, event->name);

        if (g_list_next (it) != NULL)
            fprintf (fp, ",");
    }

    fprintf (fp, "] }");
    fclose (fp);
    g_date_time_unref (now);
    g_free (timestr);
    g_free (filename);
}

void
ufo_write_opencl_events (GList *nodes)
{
    GList *it;
    EventContainer container;

    container.events = NULL;

    g_list_for (nodes, it) {
        UfoProfiler *profiler;

        profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (it->data));
        ufo_profiler_foreach (profiler, (UfoProfilerFunc) add_events, &container);
    }

    container.events = g_list_sort (container.events, (GCompareFunc) compare_events);
    write_trace_json ("opencl.%s.json", container.events);
    g_list_free (container.events);
}

void
ufo_write_profile_events (GList *nodes)
{
    GList *sorted;

    sorted = get_sorted_trace_events (nodes);
    write_trace_json ("trace.%s.json", sorted);
    g_list_foreach (sorted, (GFunc) g_free, NULL);
    g_list_free (sorted);
}
