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

 #include <ufo/ufo-messenger-iface.h>
 #include <string.h>
 #include <stdio.h>

typedef UfoMessengerIface UfoMessengerInterface;
static GTimer *global_clock;

void
ufo_message_free (UfoMessage *msg)
{
    if (msg == NULL)
        return;
    g_free (msg->data);
    g_free (msg);
}

/**
 * ufo_message_new: (skip)
 * @type: message type
 * @data_size: total size of the message
 *
 * Create a new message.
 */
UfoMessage *
ufo_message_new (UfoMessageType type, guint64 data_size)
{
    if (global_clock == NULL)
        global_clock = g_timer_new ();
   
    UfoMessage *msg = g_malloc (sizeof (UfoMessage));
    msg->type = type;
    msg->data_size = data_size;
    if (data_size == 0)
        msg->data = NULL;
    else
        msg->data = g_malloc (data_size);
    return msg;
}

gchar * ufo_message_type_to_char (UfoMessageType type)
{
    switch (type) {
        case UFO_MESSAGE_GET_NUM_DEVICES:
            return g_strdup ("UFO_MESSAGE_GET_NUM_DEVICES");
        case UFO_MESSAGE_STREAM_JSON:
            return g_strdup ("UFO_MESSAGE_STREAM_JSON");
        case UFO_MESSAGE_REPLICATE_JSON:
            return g_strdup ("UFO_MESSAGE_REPLICATE_JSON");
        case UFO_MESSAGE_GET_STRUCTURE:
            return g_strdup ("UFO_MESSAGE_GET_STRUCTURE");
        case UFO_MESSAGE_SEND_INPUTS_REQUISITION:
            return g_strdup ("UFO_MESSAGE_SEND_INPUTS_REQUISITION");
        case UFO_MESSAGE_SEND_INPUTS_DATA:
            return g_strdup ("UFO_MESSAGE_SEND_INPUTS_DATA");
        case UFO_MESSAGE_GET_REQUISITION:
            return g_strdup ("UFO_MESSAGE_GET_REQUISITION");
        case UFO_MESSAGE_GET_RESULT:
            return g_strdup ("UFO_MESSAGE_GET_RESULT");
        case UFO_MESSAGE_CLEANUP:
            return g_strdup ("UFO_MESSAGE_CLEANUP");
        case UFO_MESSAGE_TERMINATE:
            return g_strdup ("UFO_MESSAGE_TERMINATE");
        case UFO_MESSAGE_ACK:
            return g_strdup ("UFO_MESSAGE_ACK");
        default:
            return g_strdup ("UNKNOWN - NOT MAPPED");
    }
}

static GTimer *global_clock = NULL;
static GStaticMutex static_mutex = G_STATIC_MUTEX_INIT;

typedef struct {
    UfoMessengerRole role;
    gchar *addr;
    GList *events;
} NetworkProfiler;



static gchar *
get_ip_and_port (gchar *str)
{
    gsize len = strlen (str);
    if (len == 0 || !g_str_has_prefix(str, "tcp://"))
        return g_strdup (str);

    gchar *rev = g_strreverse (g_strdup(str));
    gchar *addr = g_strndup (rev, len - 6);
    g_strreverse (addr);
    return g_strdup (addr);
}

static NetworkProfiler *
init_profiler (UfoMessenger *msger, gchar *addr, UfoMessengerRole role)
{
    NetworkProfiler *p = g_malloc0 (sizeof(NetworkProfiler));
    p->events = NULL;
    p->addr = get_ip_and_port (addr);
    p->role = role;
    gpointer data = (gpointer) p;
    ufo_messenger_set_profiler (msger, data);
    return p;
}

NetworkEvent * start_trace_event (UfoMessenger *msger, UfoMessage *msg, const gchar *topic)
{
    NetworkProfiler *data = (NetworkProfiler *) ufo_messenger_get_profiler (msger);
    if (data == NULL)
        return NULL;

    NetworkEvent *ev = g_malloc0 (sizeof (NetworkEvent));
    ev->size_req = 0; ev->size_resp = 0;
    ev->timestamp_start = g_timer_elapsed (global_clock, NULL);
    ev->role = g_strdup ("MSG");
    ev->type = -1;

    if (topic != NULL) {
        ev->role = g_strdup (topic);
    }
    if (msg != NULL) {
        ev->size_req = msg->data_size;
        ev->type = msg->type;
    }
    return ev;
}

void
stop_trace_event (UfoMessenger *msger, UfoMessage *msg, NetworkEvent *ev)
{
    if (ev == NULL) return;
    NetworkProfiler *p = (NetworkProfiler*) ufo_messenger_get_profiler (msger);
    if (p == NULL) return;

    ev->timestamp_end = g_timer_elapsed (global_clock, NULL);
    ev->size_resp = 0;

    if (msg != NULL) {
        if ((gint)ev->type == -1)
            ev->type = msg->type;
        ev->size_resp = msg->data_size;
    }

    //g_debug ("%.4f %.4f %s", ev->timestamp_start, ev->timestamp_end, ufo_message_type_to_char (ev->type));

    p->events = g_list_append (p->events, ev);
}

static gint
compare_network_event (const NetworkEvent *a,
                       const NetworkEvent *b,
                       gpointer unused)
{
    return (gint) (a->timestamp_start - b->timestamp_start);
}

static GList *
get_sorted_events (NetworkProfiler *profiler)
{
    GList *events = g_list_copy (profiler->events);
    GList *sorted_events = g_list_sort (events, (GCompareFunc) compare_network_event);
    return sorted_events;
}

static void write_events_csv (UfoMessenger *msger)
{
#ifndef DEBUG
    gboolean already_written = FALSE;
#else
    gboolean already_written = TRUE;
#endif

    GMutex *mutex = g_static_mutex_get_mutex (&static_mutex);
    g_mutex_lock (mutex);

    if (already_written) {
        g_mutex_unlock (mutex);
        return;
    }
    already_written = TRUE;

    if (msger == NULL) return;
    NetworkProfiler *p = (NetworkProfiler*) ufo_messenger_get_profiler (msger);
    if (p == NULL) return;
    GList *events = get_sorted_events (p);
    if (g_list_length (events) <= 30)
        return;

    gchar *filename_base = g_strdup ("trace-messenger");
    gchar *role, *filename;
    if (p->role == UFO_MESSENGER_CLIENT)
        role = g_strdup ("CLIENT");
    else
        role = g_strdup ("SERVER");

    if (p->addr != NULL)
        filename = g_strdup_printf("%s-%s-%s-%p.csv", filename_base, role, p->addr, (void*) g_thread_self());
    else
        filename = g_strdup_printf("%s-%s-%p.csv", filename_base, role, (void*) g_thread_self());

    FILE *fp = fopen (filename, "w");

    gsize total_data = 0;
    for (GList *it = g_list_first (p->events); it != NULL; it = g_list_next (it)) {
        NetworkEvent *ev = (NetworkEvent *) it->data;
        gsize combined_data = ev->size_req + ev->size_resp;
        total_data += combined_data;
        ev->duration = ev->timestamp_end - ev->timestamp_start;
        gchar *type = ufo_message_type_to_char (ev->type);

        fprintf (fp, "%.4f\t%.4f\t%.8f\t%s\t%s\t%lu\t%lu\t%lu\t%lu\n", ev->timestamp_start,
                 ev->timestamp_end, ev->duration, type, ev->role,
                 ev->size_req, ev->size_resp, combined_data, total_data);
        g_free (type);
    }
    fclose (fp);
    g_free (filename_base);
    g_free (filename);
    g_mutex_unlock (mutex);
}

G_DEFINE_INTERFACE (UfoMessenger, ufo_messenger, G_TYPE_OBJECT)

/**
 * UfoTaskError:
 * @UFO_TASK_ERROR_SETUP: Error during setup of a task.
 */
GQuark
ufo_messenger_error_quark ()
{
    return g_quark_from_static_string ("ufo-messenger-error-quark");
}

/**
 * ufo_messenger_connect:
 * @msger: The messenger object
 * @addr: (transfer none) : The address to connect. This is implementation specific.
 * @role: The role of the local endpoint (client or server).
 *
 * Connects a messenger to and endpoint.
 */
void
ufo_messenger_connect (UfoMessenger *msger,
                       gchar *addr,
                       UfoMessengerRole role)
{
    /* iface members are not thread safe, but the daemon uses 2 messsengers
     * (one for termination that we are not interested in that is created last)
     * we use a static lock for connect/disconnect which should not affect performance
     * and only profile the first messenger that is created
    */
    if (global_clock == NULL)
        global_clock = g_timer_new ();
    init_profiler (msger, addr, role);
    UFO_MESSENGER_GET_IFACE (msger)->connect (msger, addr, role);
}

void
ufo_messenger_disconnect (UfoMessenger *msger)
{

    // TMERGE TODO conditioanl tracing
    write_events_csv (msger);

    UFO_MESSENGER_GET_IFACE (msger)->disconnect (msger);

}

/**
 * ufo_messenger_send_blocking: (skip)
 * @msger: The messenger object
 * @request: (transfer none): The request #UfoMessage.
 * @error: A #GError
 * Returns: (allow-none) : A #UfoMessage response to the sent request.
 *
 * Sends a #UfoMessage request to the connected
 * endpoint and blocks until the message was fully sent.
 */
UfoMessage *
ufo_messenger_send_blocking (UfoMessenger *msger,
                             UfoMessage *request,
                             GError **error)
{
    UfoMessage *msg = UFO_MESSENGER_GET_IFACE (msger)->send_blocking (msger, request, error);
    return msg;
}

/**
 * ufo_messenger_recv_blocking: (skip)
 * @msger: The messenger object.
 * @error: The #GError object
 *
 * Returns: The received #UfoMessage.
 *
 * Receives a #UfoMessage from the connected endpoint and blocks until the
 * message was fully received.
 *
 */
UfoMessage *
ufo_messenger_recv_blocking (UfoMessenger *msger,
                            GError **error)
{
    UfoMessage *msg = UFO_MESSENGER_GET_IFACE (msger)->recv_blocking (msger, error);
    return msg;
}

gpointer
ufo_messenger_get_profiler (UfoMessenger *msger)
{
    return UFO_MESSENGER_GET_IFACE (msger)->get_profiler (msger);
}
void
ufo_messenger_set_profiler (UfoMessenger *msger, gpointer data)
{
    UFO_MESSENGER_GET_IFACE (msger)->set_profiler (msger, data);
}
static void
ufo_messenger_default_init (UfoMessengerInterface *iface)
{
}
