#include <stdio.h>
#include "ufo-priv.h"
#include "ufo/compat.h"
#include "ufo/ufo-profiler.h"
#include "ufo/ufo-task-node.h"


typedef struct {
    UfoTraceEvent *event;
    UfoTaskNode *node;
    gdouble timestamp;
} SortedEvent;


static gint
compare_event (const SortedEvent *a,
               const SortedEvent *b,
               gpointer user_data)
{
    if (a->node != b->node || a->timestamp != b->timestamp)
        return (gint) (a->timestamp - b->timestamp);

    return (gint) ((a->event->type & UFO_TRACE_EVENT_TIME_MASK) - (b->event->type & UFO_TRACE_EVENT_TIME_MASK));
}

static GList *
get_sorted_events (GList *nodes)
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
            UfoTraceEvent *event;
            SortedEvent *new_event;

            event = (UfoTraceEvent *) jt->data;
            new_event = g_new0 (SortedEvent, 1);
            new_event->event = event;
            new_event->timestamp = event->timestamp;
            new_event->node = node;
            sorted = g_list_insert_sorted_with_data (sorted, new_event,
                                                     (GCompareDataFunc) compare_event, NULL);
        }
    }

    return sorted;
}

static void
write_opencl_row (const gchar *row, FILE *fp)
{
    fprintf (fp, "%s\n", row);
}

void
ufo_write_opencl_events (GList *nodes)
{
    FILE *fp;
    gchar *filename;
    GList *it;
    guint pid;

    pid = (guint) getpid ();

    filename = g_strdup_printf (".opencl.%i.txt", pid);
    fp = fopen (filename, "w");

    g_list_for (nodes, it) {
        UfoProfiler *profiler;

        profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (it->data));
        ufo_profiler_foreach (profiler, (UfoProfilerFunc) write_opencl_row, fp);
    }

    fclose (fp);
    g_free (filename);
}

void
ufo_write_profile_events (GList *nodes)
{
    FILE *fp;
    gchar *filename;
    GList *sorted;
    GList *it;
    guint pid;

    const gchar *event_names[] = { "process", "generate" };
    gchar event_times[] = { 'B', 'E' };
    pid = (guint) getpid ();

    sorted = get_sorted_events (nodes);
    filename = g_strdup_printf (".trace.%i.json", pid);
    fp = fopen (filename, "w");
    fprintf (fp, "{ \"traceEvents\": [");

    g_list_for (sorted, it) {
        SortedEvent *sorted_event;
        UfoTraceEvent *event;
        const gchar *name;
        gchar when;
        gchar *tid;

        sorted_event = (SortedEvent *) it->data;

        event = sorted_event->event;
        name = event_names[(event->type & UFO_TRACE_EVENT_TYPE_MASK) >> 1];
        when = event_times[(event->type & UFO_TRACE_EVENT_TIME_MASK) >> 3];
        tid = g_strdup_printf ("%s-%p", G_OBJECT_TYPE_NAME (sorted_event->node),
                                (gpointer) sorted_event->node);

        fprintf (fp, "{\"cat\":\"f\",\"ph\": \"%c\", \"ts\": %.1f, \"pid\": %i, \"tid\": \"%s\",\"name\": \"%s\", \"args\": {}}\n",
                     when, event->timestamp, pid, tid, name);

        if (g_list_next (it) != NULL)
            fprintf (fp, ",");

        g_free (tid);
    }

    fprintf (fp, "] }");
    fclose (fp);

    g_list_foreach (sorted, (GFunc) g_free, NULL);
    g_list_free (sorted);
}
