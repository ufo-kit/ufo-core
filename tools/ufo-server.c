/*
 * Copyright (C) 2011-2013 Karlsruhe Institute of Technology
 *
 * This file is part of ufod.
 *
 * ufod is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * ufod is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with ufod.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include <zmq.h>
#include <string.h>
#include <ufo/ufo.h>

typedef struct {
    UfoConfig *config;
    UfoPluginManager *manager;
    UfoTaskGraph *task_graph;
    UfoScheduler *scheduler;
    gpointer socket;
    UfoNode *input_task;
    UfoNode *output_task;
    UfoBuffer *input;
} ServerPrivate;

typedef struct {
    gchar **paths;
    gchar *addr;
} Options;

#define CHECK_ZMQ(r) if (r == -1) g_warning ("%s:%i: zmq_error: %s\n", __FILE__, __LINE__, zmq_strerror (errno));

static gpointer run_scheduler (ServerPrivate *priv);

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
handle_get_num_devices (ServerPrivate *priv)
{
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

static void
handle_json (ServerPrivate *priv)
{
    zmq_msg_t json_msg;
    gsize size;
    gchar *json;
    GList *roots;
    GList *leaves;
    UfoNode *first;
    UfoNode *last;
    GError *error = NULL;

    zmq_msg_init (&json_msg);
    size = (gsize) zmq_msg_recv (&json_msg, priv->socket, 0);

    json = g_malloc0 (size + 1);
    memcpy (json, zmq_msg_data (&json_msg), size);
    zmq_msg_close (&json_msg);

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

    g_thread_create ((GThreadFunc) run_scheduler, priv, FALSE, NULL);
    g_free (json);
    send_ack (priv->socket);
}

static void
handle_setup (ServerPrivate *priv)
{
    g_message ("Setup requested");
    send_ack (priv->socket);
}

static void
handle_get_structure (ServerPrivate *priv)
{
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
handle_send_inputs (ServerPrivate *priv)
{
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
handle_get_requisition (ServerPrivate *priv)
{
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
void handle_get_result (ServerPrivate *priv)
{
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
void handle_cleanup (ServerPrivate *priv)
{
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
run_scheduler (ServerPrivate *priv)
{
    g_message ("Start scheduler");
    ufo_scheduler_run (priv->scheduler, priv->task_graph, NULL);

    g_message ("Done");
    g_object_unref (priv->scheduler);

    priv->scheduler = ufo_scheduler_new (priv->config, NULL);
    return NULL;
}

static Options *
opts_parse (gint *argc, gchar ***argv)
{
    GOptionContext *context;
    Options *opts;
    GError *error = NULL;

    opts = g_new0 (Options, 1);

    GOptionEntry entries[] = {
        { "listen", 'l', 0, G_OPTION_ARG_STRING, &opts->addr,
          "Address to listen on (see http://api.zeromq.org/3-2:zmq-tcp)", NULL },
        { "path", 'p', 0, G_OPTION_ARG_STRING_ARRAY, &opts->paths,
          "Path to node plugins or OpenCL kernels", NULL },
        { NULL }
    };

    context = g_option_context_new ("FILE");
    g_option_context_add_main_entries (context, entries, NULL);

    if (!g_option_context_parse (context, argc, argv, &error)) {
        g_print ("Option parsing failed: %s\n", error->message);
        g_free (opts);
        return NULL;
    }

    if (opts->addr == NULL)
        opts->addr = g_strdup ("tcp://*:5555");

    g_option_context_free (context);

    return opts;
}

static UfoConfig *
opts_new_config (Options *opts)
{
    UfoConfig *config;
    GList *paths = NULL;

    config = ufo_config_new ();

    if (opts->paths != NULL) {
        for (guint i = 0; opts->paths[i] != NULL; i++)
            paths = g_list_append (paths, opts->paths[i]);

        ufo_config_add_paths (config, paths);
        g_list_free (paths);
    }

    return config;
}

static void
opts_free (Options *opts)
{
    g_strfreev (opts->paths);
    g_free (opts->addr);
    g_free (opts);
}

int
main (int argc, char * argv[])
{
    gpointer context;
    Options *opts;
    ServerPrivate priv;

    g_type_init ();
    g_thread_init (NULL);

    if ((opts = opts_parse (&argc, &argv)) == NULL)
        return 1;

    memset (&priv, 0, sizeof (ServerPrivate));

    priv.config = opts_new_config (opts);
    priv.manager = ufo_plugin_manager_new (priv.config);
    priv.scheduler = ufo_scheduler_new (priv.config, NULL);

    /* start zmq service */
    context = zmq_ctx_new ();
    priv.socket = zmq_socket (context, ZMQ_REP);
    zmq_bind (priv.socket, opts->addr);

    while (1) {
        zmq_msg_t request;

        zmq_msg_init (&request);
        zmq_msg_recv (&request, priv.socket, 0);

        if (zmq_msg_size (&request) < sizeof (UfoMessage)) {
            g_warning ("Message is smaller than expected\n");
            send_ack (priv.socket);
        }
        else {
            UfoMessage *msg;

            msg = (UfoMessage *) zmq_msg_data (&request);

            switch (msg->type) {
                case UFO_MESSAGE_GET_NUM_DEVICES:
                    handle_get_num_devices (&priv);
                    break;
                case UFO_MESSAGE_TASK_JSON:
                    handle_json (&priv);
                    break;
                case UFO_MESSAGE_SETUP:
                    handle_setup (&priv);
                    break;
                case UFO_MESSAGE_GET_STRUCTURE:
                    handle_get_structure (&priv);
                    break;
                case UFO_MESSAGE_SEND_INPUTS:
                    handle_send_inputs (&priv);
                    break;
                case UFO_MESSAGE_GET_REQUISITION:
                    handle_get_requisition (&priv);
                    break;
                case UFO_MESSAGE_GET_RESULT:
                    handle_get_result (&priv);
                    break;
                case UFO_MESSAGE_CLEANUP:
                    handle_cleanup (&priv);
                    break;
                default:
                    g_print ("unhandled case\n");
            }
        }

        zmq_msg_close (&request);
    }

    g_object_unref (priv.task_graph);
    g_object_unref (priv.config);
    g_object_unref (priv.manager);
    g_object_unref (priv.scheduler);

    zmq_close (priv.socket);
    zmq_ctx_destroy (context);

    opts_free (opts);

    return 0;
}
