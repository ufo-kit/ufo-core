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

#ifdef MPI
#include <mpi.h>
#include <ufo/ufo-mpi-messenger.h>
#endif

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ufo/ufo-config.h>
#include <ufo/ufo-daemon.h>
#include <ufo/ufo-dummy-task.h>
#include <ufo/ufo-input-task.h>
#include <ufo/ufo-output-task.h>
#include <ufo/ufo-plugin-manager.h>
#include <ufo/ufo-scheduler.h>
#include <ufo/ufo-task-graph.h>
#include <ufo/ufo-zmq-messenger.h>
#include <ufo/ufo-messenger-iface.h>

G_DEFINE_TYPE (UfoDaemon, ufo_daemon, G_TYPE_OBJECT)

#define UFO_DAEMON_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_DAEMON, UfoDaemonPrivate))

struct _UfoDaemonPrivate {
    UfoConfig *config;
    UfoPluginManager *manager;
    UfoTaskGraph *task_graph;
    UfoScheduler *scheduler;
    GThread *scheduler_thread;
    gpointer socket;
    UfoNode *input_task;
    UfoNode *output_task;
    UfoBuffer *input;
    gpointer context;
    gchar *listen_address;
    GThread *thread;
    GMutex *startstop_lock;
    GMutex *started_lock;
    GMutex *stopped_lock;
    gboolean has_started;
    gboolean has_stopped;
    GCond *started_cond;
    GCond *stopped_cond;
    UfoMessenger *msger;
};

static gpointer run_scheduler (UfoDaemon *daemon);

UfoDaemon *
ufo_daemon_new (UfoConfig *config, gchar *listen_address)
{
    UfoDaemon *daemon;

    g_return_val_if_fail (listen_address != NULL, NULL);
    g_return_val_if_fail (config != NULL, NULL);

    daemon = UFO_DAEMON (g_object_new (UFO_TYPE_DAEMON, NULL));

    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    priv->config = config;
    priv->listen_address = listen_address;
    priv->manager = ufo_plugin_manager_new (priv->config);
    priv->scheduler = ufo_scheduler_new (priv->config, NULL);
#ifdef MPI
    priv->msger = UFO_MESSENGER (ufo_mpi_messenger_new ());
#else
    priv->msger = UFO_MESSENGER (ufo_zmq_messenger_new ());
#endif
    return daemon;
}

static void
handle_get_num_devices (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    cl_context context;

    UfoMessage *msg = ufo_message_new (UFO_MESSAGE_ACK, sizeof (guint16));
    cl_uint *num_devices = g_malloc (sizeof (cl_uint));
    context = ufo_scheduler_get_context (priv->scheduler);

    UFO_RESOURCES_CHECK_CLERR (clGetContextInfo (context,
                               CL_CONTEXT_NUM_DEVICES,
                               sizeof (cl_uint),
                               num_devices,
                               NULL));

    *(guint16 *) msg->data = (guint16) *num_devices;

    ufo_messenger_send_blocking (priv->msger, msg, 0);
    ufo_message_free (msg);
}

static void
handle_get_num_cpus (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);

    UfoMessage *msg = ufo_message_new (UFO_MESSAGE_ACK, sizeof (guint16));
    *(guint16 *) msg->data = (guint16) sysconf(_SC_NPROCESSORS_ONLN);

    ufo_messenger_send_blocking (priv->msger, msg, 0);
    ufo_message_free (msg);
}

static UfoNode *
remove_dummy_if_present (UfoGraph *graph,
                         UfoNode *first)
{
    UfoNode *real = first;

    if (UFO_IS_DUMMY_TASK (first)) {
        UfoNode *dummy;
        GList *successors;

        dummy = first;
        successors = ufo_graph_get_successors (graph, dummy);
        g_assert (g_list_length (successors) == 1);
        real = UFO_NODE (successors->data);
        g_list_free (successors);
        ufo_graph_remove_edge (graph, dummy, real);
    }

    return real;
}

static gchar *
read_json (UfoDaemon *daemon, UfoMessage *msg)
{
    gchar *json;

    json = g_malloc0 (msg->data_size + 1);
    memcpy (json, msg->data, msg->data_size);

    return json;
}

static void
handle_replicate_json (UfoDaemon *daemon, UfoMessage *msg)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    gchar *json;
    UfoTaskGraph *graph;
    GError *error = NULL;

    json = read_json (daemon, msg);

    // send ack
    UfoMessage *response = ufo_message_new (UFO_MESSAGE_ACK, 0);
    ufo_messenger_send_blocking (priv->msger, response, NULL);
    ufo_message_free (response);

    graph = UFO_TASK_GRAPH (ufo_task_graph_new ());
    ufo_task_graph_read_from_data (graph, priv->manager, json, &error);

    if (error != NULL) {
        g_printerr ("%s\n", error->message);
        goto replicate_json_free;
    }

    ufo_scheduler_run (priv->scheduler, graph, NULL);
    g_object_unref (priv->scheduler);

    priv->scheduler = ufo_scheduler_new (priv->config, NULL);

replicate_json_free:
    g_object_unref (graph);
    g_free (json);
}

static void
handle_stream_json (UfoDaemon *daemon, UfoMessage *msg)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    gchar *json;
    GList *roots;
    GList *leaves;
    UfoNode *first;
    UfoNode *last;
    GError *error = NULL;

    json = read_json (daemon, msg);
    // send ack
    UfoMessage *response = ufo_message_new (UFO_MESSAGE_ACK, 0);
    ufo_messenger_send_blocking (priv->msger, response, NULL);
    ufo_message_free (response);

    /* Setup local task graph */
    priv->task_graph = UFO_TASK_GRAPH (ufo_task_graph_new ());
    ufo_task_graph_read_from_data (priv->task_graph, priv->manager, json, &error);

    if (error != NULL) {
        g_printerr ("%s\n", error->message);
        /* Send error to master */
        return;
    }

    roots = ufo_graph_get_roots (UFO_GRAPH (priv->task_graph));
    g_assert (g_list_length (roots) == 1);

    leaves = ufo_graph_get_leaves (UFO_GRAPH (priv->task_graph));
    g_assert (g_list_length (leaves) == 1);

    first = UFO_NODE (g_list_nth_data (roots, 0));
    last = UFO_NODE (g_list_nth_data (leaves, 0));

    first = remove_dummy_if_present (UFO_GRAPH (priv->task_graph), first);

    priv->input_task = ufo_input_task_new ();
    priv->output_task = ufo_output_task_new (2);

    ufo_graph_connect_nodes (UFO_GRAPH (priv->task_graph),
                             priv->input_task, first,
                             GINT_TO_POINTER (0));

    ufo_graph_connect_nodes (UFO_GRAPH (priv->task_graph),
                             last, priv->output_task,
                             GINT_TO_POINTER (0));

    priv->scheduler_thread = g_thread_create ((GThreadFunc) run_scheduler, daemon, TRUE, NULL);
    g_free (json);
    g_debug("end of handle stream json");
}

static void
handle_get_structure (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    UfoMessage *response;

    /* TODO move into .h and share between daemon and remote-node */
    struct _Structure {
        guint16 n_inputs;
        guint16 n_dims;
    } msg_data;

    /* TODO don't hardcode these */
    msg_data.n_inputs = 1;
    msg_data.n_dims = 2;

    response = ufo_message_new (UFO_MESSAGE_ACK, sizeof (struct _Structure));
    *(struct _Structure *) (response->data) = msg_data;

    ufo_messenger_send_blocking (priv->msger, response, NULL);
    ufo_message_free (response);
}

static void
handle_send_inputs (UfoDaemon *daemon, UfoMessage *request)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    UfoRequisition requisition;
    gpointer context;

    context = ufo_scheduler_get_context (priv->scheduler);

    struct _Header {
        UfoRequisition requisition;
        guint64 buffer_size;
    };

    char *base = request->data;
    struct _Header *header = (struct _Header *) base;

    /* Receive buffer size */
    requisition = header->requisition;
    if (priv->input == NULL) {
        priv->input = ufo_buffer_new (&requisition, context);
    }
    else {
        if (ufo_buffer_cmp_dimensions (priv->input, &requisition))
            ufo_buffer_resize (priv->input, &requisition);
    }
    memcpy (ufo_buffer_get_host_array (priv->input, NULL),
            base + sizeof (struct _Header),
            ufo_buffer_get_size (priv->input));
    ufo_input_task_release_input_buffer (UFO_INPUT_TASK (priv->input_task), priv->input);

    UfoMessage *response = ufo_message_new (UFO_MESSAGE_ACK, 0);
    ufo_messenger_send_blocking (priv->msger, response, NULL);
    ufo_message_free (response);
}

static void
handle_get_requisition (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    UfoRequisition requisition;

    /* We need to get the requisition from the last node */
    ufo_output_task_get_output_requisition (UFO_OUTPUT_TASK (priv->output_task),
                                            &requisition);

    UfoMessage *msg = ufo_message_new (UFO_MESSAGE_ACK, sizeof (UfoRequisition));
    memcpy (msg->data, &requisition, msg->data_size);
    ufo_messenger_send_blocking (priv->msger, msg, NULL);
    ufo_message_free (msg);
}

static
void handle_get_result (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    UfoBuffer *buffer;
    gsize size;

    buffer = ufo_output_task_get_output_buffer (UFO_OUTPUT_TASK (priv->output_task));
    size = ufo_buffer_get_size (buffer);

    UfoMessage *response = ufo_message_new (UFO_MESSAGE_ACK, size);
    memcpy (response->data, ufo_buffer_get_host_array (buffer, NULL), size);
    ufo_messenger_send_blocking (priv->msger, response, NULL);
    ufo_output_task_release_output_buffer (UFO_OUTPUT_TASK (priv->output_task), buffer);
}

static void
unref_and_free (GObject **object)
{
    if (*object) {
        g_object_unref (*object);
        *object = NULL;
    }
}

static
void handle_cleanup (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);

    /*
     * We send the ACK early on, because we don't want to let the host wait for
     * actually cleaning up (and waiting some time to unref the input task).
     */
    UfoMessage *response = ufo_message_new (UFO_MESSAGE_ACK, 0);
    ufo_messenger_send_blocking (priv->msger, response, NULL);
    ufo_message_free (response);

    // TODO check that we don't need to execture this branch wen priv->input is null
    if (priv->input_task && priv->input) {
        ufo_input_task_stop (UFO_INPUT_TASK (priv->input_task));

        ufo_input_task_release_input_buffer (UFO_INPUT_TASK (priv->input_task),
                                             priv->input);

        g_usleep (1.5 * G_USEC_PER_SEC);
        unref_and_free ((GObject **) &priv->input_task);
        unref_and_free ((GObject **) &priv->input);
    }

    unref_and_free ((GObject **) &priv->output_task);
    unref_and_free ((GObject **) &priv->task_graph);

}

static void
handle_terminate (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    UfoMessage *response = ufo_message_new (UFO_MESSAGE_ACK, 0);
    ufo_messenger_send_blocking (priv->msger, response, NULL);
    ufo_message_free (response);

    ufo_messenger_disconnect (priv->msger);

    if(priv->scheduler_thread != NULL) {
        g_message ("waiting for scheduler to finish");
        g_thread_join (priv->scheduler_thread);
        g_message ("scheduler finished!");
    }
}

static gpointer
run_scheduler (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    g_message ("Start scheduler");
    ufo_scheduler_run (priv->scheduler, priv->task_graph, NULL);

    g_message ("Done");
    g_object_unref (priv->scheduler);

    priv->scheduler = ufo_scheduler_new (priv->config, NULL);
    return NULL;
}

static gboolean
handle_incoming (UfoDaemon *daemon, UfoMessage *msg)
{
    // g_debug ("handling %s", ufo_message_type_to_char (msg->type));
    switch (msg->type) {
        case UFO_MESSAGE_GET_NUM_DEVICES:
            handle_get_num_devices (daemon);
            break;
        case UFO_MESSAGE_GET_NUM_CPUS:
            handle_get_num_cpus (daemon);
            break;
        case UFO_MESSAGE_STREAM_JSON:
            handle_stream_json (daemon, msg);
            break;
        case UFO_MESSAGE_REPLICATE_JSON:
            handle_replicate_json (daemon, msg);
            break;
        case UFO_MESSAGE_GET_STRUCTURE:
            handle_get_structure (daemon);
            break;
        case UFO_MESSAGE_SEND_INPUTS:
            handle_send_inputs (daemon, msg);
            break;
        case UFO_MESSAGE_GET_REQUISITION:
            handle_get_requisition (daemon);
            break;
        case UFO_MESSAGE_GET_RESULT:
            handle_get_result (daemon);
            break;
        case UFO_MESSAGE_CLEANUP:
            handle_cleanup (daemon);
            break;
        case UFO_MESSAGE_TERMINATE:
            handle_terminate (daemon);
            return FALSE;
            break;
        default:
            g_message ("Unknown message received\n");
    }
    return TRUE;
}

static void
ufo_daemon_start_impl (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    g_debug ("UfoDaemon started on address %s", priv->listen_address);

    // tell the calling thread that we have started
    g_mutex_lock (priv->started_lock);
    priv->has_started = TRUE;
    g_cond_signal (priv->started_cond);
    g_mutex_unlock (priv->started_lock);

    gboolean wait_for_messages = TRUE;
    while (wait_for_messages) {
        GError *err = NULL;
        UfoMessage *msg = ufo_messenger_recv_blocking (priv->msger, &err);
        if (err != NULL) {
            /* if daemon is stopped, socket will be closed and msg_recv
            * will yield an error - we stop
            */
            wait_for_messages = FALSE;
        } else {
            wait_for_messages = handle_incoming (daemon, msg);
            ufo_message_free (msg);
        }
    }

    // tell calling thread we have stopped
    g_mutex_lock (priv->stopped_lock);
    priv->has_stopped = TRUE;
    g_cond_signal (priv->stopped_cond);
    g_mutex_unlock (priv->stopped_lock);
}

void
ufo_daemon_start (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);

    g_mutex_lock (priv->startstop_lock);
    if (priv->has_started) {
        g_mutex_unlock (priv->startstop_lock);
        return;
    }

    /* TODO handle error if unable to connect/bind */
    ufo_messenger_connect (priv->msger, priv->listen_address, UFO_MESSENGER_SERVER);

    priv->thread = g_thread_create ((GThreadFunc)ufo_daemon_start_impl, daemon, TRUE, NULL);
    g_return_if_fail (priv->thread != NULL);

    g_mutex_lock (priv->started_lock);
    while (!priv->has_started)
        g_cond_wait (priv->started_cond, priv->started_lock);
    g_mutex_unlock (priv->started_lock);


    g_mutex_unlock (priv->startstop_lock);
}

void
ufo_daemon_stop (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    g_mutex_lock (priv->startstop_lock);

    /* HACK we can't call _disconnect() as this has to be run from the
     * thread running the daemon which might be blocking on recv
     * - we thus send a TERMINATE message to that thread
     */

    UfoMessenger *tmp_msger;
#ifdef MPI
    tmp_msger = UFO_MESSENGER (ufo_mpi_messenger_new ());
#else
    tmp_msger = UFO_MESSENGER (ufo_zmq_messenger_new ());
#endif

    ufo_messenger_connect (tmp_msger, priv->listen_address, UFO_MESSENGER_CLIENT);
    UfoMessage *request = ufo_message_new (UFO_MESSAGE_TERMINATE, 0);
    ufo_messenger_send_blocking (tmp_msger, request, NULL);
    ufo_messenger_disconnect (tmp_msger);

    g_thread_join (priv->thread);

    g_mutex_lock (priv->stopped_lock);
    priv->has_stopped = TRUE;
    g_cond_signal (priv->stopped_cond);
    g_mutex_unlock (priv->stopped_lock);

    g_mutex_unlock (priv->startstop_lock);
}

void ufo_daemon_wait_finish (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);

    g_mutex_lock (priv->stopped_lock);
    while (!priv->has_stopped)
        g_cond_wait (priv->stopped_cond, priv->stopped_lock);
    g_mutex_unlock (priv->stopped_lock);
}

static void
ufo_daemon_dispose (GObject *object)
{
    UfoDaemonPrivate *priv;
    priv = UFO_DAEMON_GET_PRIVATE (object);

    if (priv->task_graph)
        g_object_unref (priv->task_graph);
    if (priv->config != NULL)
        g_object_unref (priv->config);
    if (priv->msger != NULL)
        g_object_unref (priv->msger);
    if (priv->manager != NULL)
        g_object_unref (priv->manager);
    if (priv->scheduler != NULL)
        g_object_unref (priv->scheduler);

    G_OBJECT_CLASS (ufo_daemon_parent_class)->dispose (object);
}

static void
ufo_daemon_finalize (GObject *object)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (object);
    g_mutex_free (priv->startstop_lock);
    g_cond_free (priv->started_cond);

    G_OBJECT_CLASS (ufo_daemon_parent_class)->finalize (object);
}

static void
ufo_daemon_class_init (UfoDaemonClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);
    oclass->dispose = ufo_daemon_dispose;
    oclass->finalize = ufo_daemon_finalize;

    g_type_class_add_private (klass, sizeof (UfoDaemonPrivate));
}

static void
ufo_daemon_init (UfoDaemon *self)
{
    UfoDaemonPrivate *priv;
    self->priv = priv = UFO_DAEMON_GET_PRIVATE (self);
    priv->startstop_lock = g_mutex_new ();
    priv->started_lock = g_mutex_new ();
    priv->stopped_lock = g_mutex_new ();
    priv->started_cond = g_cond_new ();
    priv->stopped_cond = g_cond_new ();
    priv->has_started = FALSE;
    priv->has_stopped = FALSE;
}
