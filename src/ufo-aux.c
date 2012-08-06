/**
 * SECTION:ufo-aux
 * @Short_description: Auxiliary functions and macros
 * @Title: Auxiliary functions
 */

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include "ufo-aux.h"

/**
 * UfoEventList:
 *
 * An object to encapsulate %cl_event lists returned by
 * ufo_filter_process_gpu().
 */
struct _UfoEventList {
    GList       *list;
    cl_event    *events;
    guint        n_events;
};

/**
 * ufo_event_list_new: (skip)
 * @n_events: Number of %cl_events
 *
 * Create a new event list.
 *
 * Returns: A newly created event list or %NULL if #n_events is 0.
 * Since: 0.2
 */
UfoEventList *
ufo_event_list_new (guint n_events)
{
    UfoEventList *event_list;

    if (n_events == 0)
        return NULL;

    event_list = g_new0 (UfoEventList, 1);
    event_list->list = NULL;
    event_list->n_events = n_events;
    event_list->events = g_new0 (cl_event, n_events);

    for (guint i = 0; i < n_events; i++)
        event_list->list = g_list_append (event_list->list, &event_list->events[i]);

    return event_list;
}

static
void unref_cl_event (cl_event *event, gpointer user_data)
{
    clReleaseEvent (*event);
}

/**
 * ufo_event_list_free:
 * @list: A #UfoEventList
 *
 * Free's the list and reduces the reference count of the contained %cl_event
 * objects.
 *
 * Since: 0.2
 */
void
ufo_event_list_free (UfoEventList  *list)
{
    g_return_if_fail (list != NULL);

    g_list_foreach (list->list, (GFunc) unref_cl_event, NULL);

    /* FIXME: This is really strange. If we free the events array, the allocator
     * complains on ufosrv1 about freeing an undefined pointer, although we
     * really allocated it on our own in ufo_event_list_free(). */
    /* g_free (list->events); */

    g_list_free (list->list);
    g_free (list);
}

/**
 * ufo_event_list_get_event_array: (skip)
 * @list: A #UfoEventList
 *
 * Return an array of %cl_event objects.
 *
 * Returns: An array.
 * Since: 0.2
 */
gpointer
ufo_event_list_get_event_array (UfoEventList *list)
{
    return list->events;
}

/**
 * ufo_event_list_get_list: (skip)
 * @list: A #UfoEventList
 *
 * Return a #GList containing pointers to the %cl_event objects.
 *
 * Returns: (transfer none) (element-type gpointer): A #GList.
 * Since: 0.2
 */
GList *
ufo_event_list_get_list (UfoEventList *list)
{
    return list->list;
}

void
ufo_debug_cl (const gchar *format, ...)
{
    va_list args;

    va_start (args, format);
    g_logv ("ocl", G_LOG_LEVEL_MESSAGE, format, args);
    va_end (args);
}


