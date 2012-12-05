#include <zmq.h>
#include <glib-object.h>
#include <string.h>
#include <ufo-buffer.h>
#include <ufo-resources.h>
#include <ufo-task-graph.h>
#include <ufo-scheduler.h>
#include <ufo-input-task.h>
#include <ufo-output-task.h>
#include <ufo-remote-node.h>

typedef struct {
    gpointer socket;
    UfoNode *input_task;
    UfoNode *output_task;
    UfoTaskGraph *task_graph;
    UfoArchGraph *arch_graph;
    UfoBuffer **inputs;
    guint n_inputs;
} ServerPrivate;

#define CHECK_ZMQ(r) if (r == -1) g_warning ("%s:%i: zmq_error: %s\n", __FILE__, __LINE__, zmq_strerror (errno));

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
handle_setup (ServerPrivate *priv)
{
    g_print ("handle setup\n");
    send_ack (priv->socket);
}

static void
handle_get_structure (ServerPrivate *priv)
{
    UfoInputParameter *in_params;
    UfoMessage header;
    zmq_msg_t data_msg;

    g_print ("handle get_structure\n");
    in_params = g_new0 (UfoInputParameter, 1);

    header.type = UFO_MESSAGE_STRUCTURE;
    header.n_inputs = priv->n_inputs;
    in_params->n_dims = 2;
    in_params->n_expected_items = -1;

    priv->inputs = g_new0 (UfoBuffer *, priv->n_inputs);

    zmq_msg_init_size (&data_msg, sizeof (UfoInputParameter));
    memcpy (zmq_msg_data (&data_msg), in_params, sizeof (UfoInputParameter));

    ufo_msg_send (&header, priv->socket, ZMQ_SNDMORE);
    zmq_msg_send (&data_msg, priv->socket, 0);
    zmq_msg_close (&data_msg);

    g_free (in_params);
}

static void
handle_send_inputs (ServerPrivate *priv)
{
    gpointer context;

    context = ufo_arch_graph_get_context (priv->arch_graph);

    for (guint i = 0; i < priv->n_inputs; i++) {
        /* Receive buffer size */
        UfoRequisition *requisition;
        zmq_msg_t requisition_msg;
        zmq_msg_t data_msg;

        zmq_msg_init (&requisition_msg);
        zmq_msg_recv (&requisition_msg, priv->socket, 0);
        g_assert (zmq_msg_size (&requisition_msg) >= sizeof (UfoRequisition));
        requisition = zmq_msg_data (&requisition_msg);

        if (priv->inputs[i] == NULL) {
            priv->inputs[i] = ufo_buffer_new (requisition, context);
        }
        else {
            if (ufo_buffer_cmp_dimensions (priv->inputs[i], requisition))
                ufo_buffer_resize (priv->inputs[i], requisition);
        }

        zmq_msg_close (&requisition_msg);

        /* Receive actual buffer */
        zmq_msg_init (&data_msg);
        zmq_msg_recv (&data_msg, priv->socket, 0);
        memcpy (ufo_buffer_get_host_array (priv->inputs[i], NULL),
                zmq_msg_data (&data_msg),
                ufo_buffer_get_size (priv->inputs[i]));
        ufo_input_task_release_input_buffer (UFO_INPUT_TASK (priv->input_task), i, priv->inputs[i]);
        zmq_msg_close (&data_msg);
    }

    send_ack (priv->socket);
}

static void
handle_get_requisition (ServerPrivate *priv)
{
    UfoRequisition requisition;
    zmq_msg_t reply_msg;

    /* We need to get the requisition from the last node */
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

static gpointer
run_scheduler (ServerPrivate *priv)
{
    UfoScheduler *scheduler;

    scheduler = ufo_scheduler_new ();
    ufo_scheduler_run (scheduler, priv->arch_graph, priv->task_graph, NULL);
    g_object_unref (scheduler);

    return NULL;
}

int
main (int argc, char const* argv[])
{
    gpointer context;
    ServerPrivate priv;
    UfoResources *resources;
    UfoTaskGraphConnection connection;

    g_type_init ();
    g_thread_init (NULL);

    /* start zmq service */
    context = zmq_ctx_new ();
    priv.socket = zmq_socket (context, ZMQ_REP);
    zmq_bind (priv.socket, "tcp://*:5555");

    /* setup task and arch graphs */
    resources = ufo_resources_new (NULL);
    priv.arch_graph = UFO_ARCH_GRAPH (ufo_arch_graph_new (NULL, NULL, resources));
    priv.task_graph = UFO_TASK_GRAPH (ufo_task_graph_new ());

    connection.source_output = 0;
    connection.target_input = 0;

    priv.n_inputs = 1;
    priv.input_task = ufo_input_task_new (NULL);
    priv.output_task = ufo_output_task_new (2);

    ufo_graph_connect_nodes (UFO_GRAPH (priv.task_graph), priv.input_task, priv.output_task, &connection);
    g_thread_create ((GThreadFunc) run_scheduler, &priv, FALSE, NULL);

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
                default:
                    g_print ("unhandled case\n");
            }
        }

        zmq_msg_close (&request);
    }

    g_object_unref (priv.task_graph);
    g_object_unref (priv.arch_graph);
    g_object_unref (resources);

    zmq_close (priv.socket);
    zmq_ctx_destroy (context);

    return 0;
}
