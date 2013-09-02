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
#include <zmq.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <ufo/ufo-config.h>
#include <ufo/ufo-daemon.h>
#include <ufo/ufo-dummy-task.h>
#include <ufo/ufo-input-task.h>
#include <ufo/ufo-output-task.h>
#include <ufo/ufo-plugin-manager.h>
#include <ufo/ufo-scheduler.h>
#include <ufo/ufo-task-graph.h>

G_DEFINE_TYPE (UfoDaemon, ufo_daemon, G_TYPE_OBJECT)

#define UFO_DAEMON_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_DAEMON, UfoDaemonPrivate))

#define CHECK_ZMQ(r) if (r == -1) g_warning ("%s:%i: zmq_error: %s\n", __FILE__, __LINE__, zmq_strerror (errno));

struct _UfoDaemonPrivate {
    UfoConfig *config;
    UfoPluginManager *manager;
    UfoTaskGraph *task_graph;
    UfoScheduler *scheduler;
    gpointer socket;
    UfoNode *input_task;
    UfoNode *output_task;
    UfoBuffer *input;
    gpointer context;
    gchar *listen_address;
    GThread *thread;
    gboolean run;
    GMutex *started;
};

static gpointer run_scheduler (UfoDaemon *daemon);
static void validate_zmq_listen_address (gchar *addr);

UfoDaemon *
ufo_daemon_new (UfoConfig *config, gchar *listen_address)
{
    UfoDaemon *daemon;

    g_return_val_if_fail (listen_address != NULL, NULL);
    g_return_val_if_fail (config != NULL, NULL);

    validate_zmq_listen_address (listen_address);

    daemon = UFO_DAEMON (g_object_new (UFO_TYPE_DAEMON, NULL));

    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    priv->config = config;
    priv->listen_address = listen_address;
    priv->manager = ufo_plugin_manager_new (priv->config);
    priv->scheduler = ufo_scheduler_new (priv->config, NULL);
    priv->context = zmq_ctx_new ();
    priv->socket = zmq_socket (priv->context, ZMQ_REP);

    int err = zmq_bind (priv->socket, listen_address);
    if (err < 0)
        g_critical ("could not bind to address %s", listen_address);

    return daemon;
}

static void
validate_zmq_listen_address (gchar *addr)
{
    if (!g_str_has_prefix (addr, "tcp://"))
        g_critical ("address didn't start with tcp:// scheme, which is required currently");

    /* Pitfall: zmq will silently accept hostnames like tcp://localhost:5555
     * but not bind to it as it treats it like an interface name (like eth0).
     * We have to use IP addresses instead of DNS names.
     */
    gchar *host = g_strdup (&addr[6]);
    if (!g_ascii_isdigit (host[0]) && host[0] != '*')
        g_warning ("treating address %s as interface device name. Use IP address if supplying a host was intended.", host);
    g_free (host);
}

static void
ufo_msg_send (UfoMessage *msg, gpointer socket, gint flags)
{
    zmq_msg_t reply;

    zmq_msg_init_size (&reply, sizeof (UfoMessage));
    memcpy (zmq_msg_data (&reply), msg, sizeof (UfoMessage));
    zmq_msg_send (&reply, socket, flags);
    zmq_msg_close (&reply);
}

static void
send_ack (gpointer socket)
{
    UfoMessage msg;
    msg.type = UFO_MESSAGE_ACK;
    ufo_msg_send (&msg, socket, 0);
}

static void
handle_get_num_devices (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);

    UfoMessage msg;
    cl_context context;

    context = ufo_scheduler_get_context (priv->scheduler);

    UFO_RESOURCES_CHECK_CLERR (clGetContextInfo (context,
                               CL_CONTEXT_NUM_DEVICES,
                               sizeof (cl_uint),
                               &msg.d.n_devices,
                               NULL));

    ufo_msg_send (&msg, priv->socket, 0);
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
read_json (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);

    zmq_msg_t json_msg;
    gsize size;
    gchar *json;

    zmq_msg_init (&json_msg);
    size = (gsize) zmq_msg_recv (&json_msg, priv->socket, 0);

    json = g_malloc0 (size + 1);
    memcpy (json, zmq_msg_data (&json_msg), size);
    zmq_msg_close (&json_msg);

    return json;
}

static void
handle_replicate_json (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    gchar *json;
    UfoTaskGraph *graph;
    GError *error = NULL;

    json = read_json (daemon);
    send_ack (priv->socket);

    graph = UFO_TASK_GRAPH (ufo_task_graph_new ());
    ufo_task_graph_read_from_data (graph, priv->manager, json, &error);

    if (error != NULL) {
        g_printerr ("%s\n", error->message);
        goto replicate_json_free;
    }

    g_message ("Start scheduler");
    ufo_scheduler_run (priv->scheduler, graph, NULL);

    g_message ("Done");
    g_object_unref (priv->scheduler);

    priv->scheduler = ufo_scheduler_new (priv->config, NULL);

replicate_json_free:
    g_object_unref (graph);
    g_free (json);
}

static void
handle_stream_json (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    gchar *json;
    GList *roots;
    GList *leaves;
    UfoNode *first;
    UfoNode *last;
    GError *error = NULL;

    json = read_json (daemon);

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

    g_thread_create ((GThreadFunc) run_scheduler, daemon, FALSE, NULL);
    g_free (json);
    send_ack (priv->socket);
}

static void
handle_setup (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    g_message ("Setup requested");
    send_ack (priv->socket);
}

static void
handle_get_structure (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    UfoMessage header;
    UfoInputParam in_param;
    zmq_msg_t data_msg;

    g_message ("Structure requested");
    header.type = UFO_MESSAGE_STRUCTURE;

    /* TODO: do not hardcode these */
    header.d.n_inputs = 1;
    in_param.n_dims = 2;

    zmq_msg_init_size (&data_msg, sizeof (UfoInputParam));
    memcpy (zmq_msg_data (&data_msg), &in_param, sizeof (UfoInputParam));

    ufo_msg_send (&header, priv->socket, ZMQ_SNDMORE);
    zmq_msg_send (&data_msg, priv->socket, 0);
    zmq_msg_close (&data_msg);
}

static void
handle_send_inputs (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    UfoRequisition *requisition;
    zmq_msg_t requisition_msg;
    zmq_msg_t data_msg;
    gpointer context;

    context = ufo_scheduler_get_context (priv->scheduler);

    /* Receive buffer size */
    zmq_msg_init (&requisition_msg);
    zmq_msg_recv (&requisition_msg, priv->socket, 0);
    g_assert (zmq_msg_size (&requisition_msg) >= sizeof (UfoRequisition));
    requisition = zmq_msg_data (&requisition_msg);

    if (priv->input == NULL) {
        priv->input = ufo_buffer_new (requisition, context);
    }
    else {
        if (ufo_buffer_cmp_dimensions (priv->input, requisition))
            ufo_buffer_resize (priv->input, requisition);
    }

    zmq_msg_close (&requisition_msg);

    /* Receive actual buffer */
    zmq_msg_init (&data_msg);
    zmq_msg_recv (&data_msg, priv->socket, 0);

    memcpy (ufo_buffer_get_host_array (priv->input, NULL),
            zmq_msg_data (&data_msg),
            ufo_buffer_get_size (priv->input));

    ufo_input_task_release_input_buffer (UFO_INPUT_TASK (priv->input_task), priv->input);
    zmq_msg_close (&data_msg);

    send_ack (priv->socket);
}

static void
handle_get_requisition (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    UfoRequisition requisition;
    zmq_msg_t reply_msg;

    /* We need to get the requisition from the last node */
    g_message ("Requisition requested");
    ufo_output_task_get_output_requisition (UFO_OUTPUT_TASK (priv->output_task),
                                            &requisition);

    zmq_msg_init_size (&reply_msg, sizeof (UfoRequisition));
    memcpy (zmq_msg_data (&reply_msg), &requisition, sizeof (UfoRequisition));
    zmq_msg_send (&reply_msg, priv->socket, 0);
    zmq_msg_close (&reply_msg);
}

static
void handle_get_result (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    UfoBuffer *buffer;
    zmq_msg_t reply_msg;
    gsize size;

    buffer = ufo_output_task_get_output_buffer (UFO_OUTPUT_TASK (priv->output_task));
    size = ufo_buffer_get_size (buffer);

    zmq_msg_init_size (&reply_msg, size);
    memcpy (zmq_msg_data (&reply_msg), ufo_buffer_get_host_array (buffer, NULL), size);
    zmq_msg_send (&reply_msg, priv->socket, 0);
    zmq_msg_close (&reply_msg);
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
    send_ack (priv->socket);

    if (priv->input_task) {
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

static void
ufo_daemon_start_impl (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    while (priv->run) {
        zmq_msg_t request;

        zmq_msg_init (&request);

        g_mutex_unlock (priv->started);
        gint err = zmq_msg_recv (&request, priv->socket, 0);
        /* if daemon is stopped, socket will be closed and msg_recv
         * will yield an error - we simply want to return
         */
        if (err < 0) return;

        if (zmq_msg_size (&request) < sizeof (UfoMessage)) {
            g_warning ("Message is smaller than expected\n");
            send_ack (priv->socket);
        }
        else {
            UfoMessage *msg;

            msg = (UfoMessage *) zmq_msg_data (&request);

            switch (msg->type) {
                case UFO_MESSAGE_GET_NUM_DEVICES:
                    handle_get_num_devices (daemon);
                    break;
                case UFO_MESSAGE_STREAM_JSON:
                    handle_stream_json (daemon);
                    break;
                case UFO_MESSAGE_REPLICATE_JSON:
                    handle_replicate_json (daemon);
                    break;
                case UFO_MESSAGE_SETUP:
                    handle_setup (daemon);
                    break;
                case UFO_MESSAGE_GET_STRUCTURE:
                    handle_get_structure (daemon);
                    break;
                case UFO_MESSAGE_SEND_INPUTS:
                    handle_send_inputs (daemon);
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
                default:
                    g_print ("unhandled case\n");
            }
        }
        zmq_msg_close (&request);
    }
}

GThread *
ufo_daemon_start (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);

    // acquire lock that is released after thread started
    g_mutex_lock (priv->started);
    priv->run = TRUE;

    priv->thread = g_thread_create ((GThreadFunc)ufo_daemon_start_impl, daemon, TRUE, NULL);
    g_return_val_if_fail (priv->thread != NULL, NULL);

    // increase the ref so that we can call g_thread_join on it
    g_thread_ref (priv->thread);

    // wait for the thread to start listening by re-acquiring the lock
    g_mutex_lock (priv->started);
    g_mutex_unlock (priv->started);

    return priv->thread;
}

void
ufo_daemon_stop (UfoDaemon *daemon)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (daemon);
    priv->run = FALSE;

    zmq_close (priv->socket);

    if (priv->context != NULL) {
        zmq_ctx_destroy (priv->context);
        priv->context = NULL;
        g_assert (priv->context == NULL);
    }

    g_thread_unref(priv->thread);
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
    if (priv->manager != NULL)
        g_object_unref (priv->manager);
    if (priv->scheduler != NULL)
        g_object_unref (priv->scheduler);

    zmq_close (priv->socket);

    if (priv->context != NULL) {
        zmq_ctx_destroy (priv->context);
        priv->context = NULL;
    }

    G_OBJECT_CLASS (ufo_daemon_parent_class)->dispose (object);
}

static void
ufo_daemon_finalize (GObject *object)
{
    UfoDaemonPrivate *priv = UFO_DAEMON_GET_PRIVATE (object);
    g_mutex_free (priv->started);

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
    priv->started = g_mutex_new ();
}
