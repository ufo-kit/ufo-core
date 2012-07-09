
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include "ufo-aux.h"

struct _UfoEventList {
    GList       *list;
    cl_event    *events;
    guint        n_events;
};

UfoEventList *
ufo_event_list_new (guint n_events)
{
    UfoEventList *event_list;

    event_list = g_new0 (UfoEventList, 1);
    event_list->list = NULL;
    event_list->n_events = n_events;
    event_list->events = g_new0 (cl_event, n_events);

    for (guint i = 0; i < n_events; i++)
        event_list->list = g_list_append (event_list->list, &event_list->events[i]);

    return event_list;
}

void
ufo_event_list_free (UfoEventList  *list)
{
    g_list_free (list->list);
    g_free (list->events);
    g_free (list);
}

gpointer
ufo_event_list_get_event_array (UfoEventList *list)
{
    return list->events;
}

GList *
ufo_event_list_get_list (UfoEventList *list)
{
    return list->list;
}
