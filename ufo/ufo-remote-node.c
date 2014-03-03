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

#include <string.h>
#include <ufo/ufo-remote-node.h>
#include <ufo/ufo-messenger-iface.h>
#include <ufo/ufo-zmq-messenger.h>

#ifdef MPI
#include <ufo/ufo-mpi-messenger.h>
#endif

G_DEFINE_TYPE (UfoRemoteNode, ufo_remote_node, UFO_TYPE_NODE)

#define UFO_REMOTE_NODE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_REMOTE_NODE, UfoRemoteNodePrivate))

struct _UfoRemoteNodePrivate {
    gpointer context;
    guint n_inputs;
    gboolean terminated;
    UfoMessenger *msger;
    gchar *addr;
};

static GTimer *global_clock;

typedef struct {
	gfloat start;
	gchar *msg;
} TraceHandle;

static TraceHandle * trace_start (const gchar *msg)
{
	TraceHandle *th = g_malloc (sizeof(TraceHandle));
	th->msg = g_strdup (msg);
	th->start = g_timer_elapsed (global_clock, NULL);
	return th;
}
static void trace_stop (TraceHandle *th)
{
	gfloat now = g_timer_elapsed (global_clock, NULL);
	gfloat delta = now - th->start;
	g_debug ("%.6f %s", delta, th->msg);
	g_free (th->msg);
	g_free (th);
}

UfoNode *
ufo_remote_node_new (const gchar *address)
{
    UfoRemoteNode *node;
    UfoRemoteNodePrivate *priv;

    g_return_val_if_fail (address != NULL, NULL);
    node = UFO_REMOTE_NODE (g_object_new (UFO_TYPE_REMOTE_NODE, NULL));
    priv = UFO_REMOTE_NODE_GET_PRIVATE (node);

#ifdef MPI
    priv->msger = UFO_MESSENGER (ufo_mpi_messenger_new ());
#else
    priv->msger = UFO_MESSENGER (ufo_zmq_messenger_new ());
#endif

    gchar *addr = g_strdup (address);
    ufo_messenger_connect (priv->msger, addr, UFO_MESSENGER_CLIENT);
    priv->addr = addr;
    // g_free(addr);

    return UFO_NODE (node);
}

guint
ufo_remote_node_get_num_gpus (UfoRemoteNode *node)
{
    g_return_val_if_fail (UFO_IS_REMOTE_NODE (node), 0);

    UfoRemoteNodePrivate *priv;
    UfoMessage *request = ufo_message_new (UFO_MESSAGE_GET_NUM_DEVICES, 0);

    priv = node->priv;

    UfoMessage *result;
    result = ufo_messenger_send_blocking (priv->msger, request, NULL);
    guint n_devices = * (guint16 *) result->data;

    ufo_message_free (request);
    ufo_message_free (result);
    g_assert (n_devices < 32);
    return n_devices;
}

guint
ufo_remote_node_get_num_cpus (UfoRemoteNode *node)
{
    g_return_val_if_fail (UFO_IS_REMOTE_NODE (node), 0);

    UfoRemoteNodePrivate *priv = node->priv;
    UfoMessage *request = ufo_message_new (UFO_MESSAGE_GET_NUM_CPUS, 0);

    UfoMessage *result;
    result = ufo_messenger_send_blocking (priv->msger, request, NULL);
    guint n_devices = * (guint16 *) result->data;

    ufo_message_free (request);
    ufo_message_free (result);
    g_assert (n_devices > 0);
    return n_devices;
}

void
ufo_remote_node_request_setup (UfoRemoteNode *node)
{
    // TODO setup isn't in use, remove it
    //g_assert (FALSE);

    // g_return_if_fail (UFO_IS_REMOTE_NODE (node));
    // UfoRemoteNodePrivate *priv = UFO_REMOTE_NODE_GET_PRIVATE (node);

    // UfoMessage *request;
    // request = ufo_message_new (UFO_MESSAGE_SETUP, 0);
    // ufo_message_send_blocking (request);
    // ufo_message_free (request);
}

void
ufo_remote_node_send_json (UfoRemoteNode *node,
                           UfoRemoteMode mode,
                           const gchar *json)
{
    UfoRemoteNodePrivate *priv;
    UfoMessage *request;
    UfoMessageType type;
    guint64 size;

    g_return_if_fail (UFO_IS_REMOTE_NODE (node));

    priv = node->priv;

    switch (mode) {
        case UFO_REMOTE_MODE_STREAM:
            type = UFO_MESSAGE_STREAM_JSON;
            break;
        case UFO_REMOTE_MODE_REPLICATE:
            type = UFO_MESSAGE_REPLICATE_JSON;
            break;
        default:
            g_warning ("Don't understand UfoRemoteMode");
            type = 0;
    }

    size = (guint64) strlen (json);
    request = ufo_message_new (type, size);

    memcpy (request->data, json, size);
    ufo_messenger_send_blocking (priv->msger, request, NULL);
}

void
ufo_remote_node_get_structure (UfoRemoteNode *node,
                               guint *n_inputs,
                               UfoInputParam **in_params,
                               UfoTaskMode *mode)
{
    UfoRemoteNodePrivate *priv;
    UfoMessage *request, *response;

    priv = node->priv;

    struct _Structure {
        guint16 n_inputs;
        guint16 n_dims;
        UfoTaskMode mode;
    } msg_data;

    g_return_if_fail (UFO_IS_REMOTE_NODE (node));

    request = ufo_message_new (UFO_MESSAGE_GET_STRUCTURE, 0);
    response = ufo_messenger_send_blocking (priv->msger, request, NULL);
    g_assert (response->data_size == sizeof (struct _Structure));

    msg_data = *(struct _Structure *) response->data;

    *mode = msg_data.mode;

    priv->n_inputs = msg_data.n_inputs;
    *n_inputs = msg_data.n_inputs;

    *in_params = g_new0 (UfoInputParam, 1);
    (*in_params)[0].n_dims = msg_data.n_dims;

    ufo_message_free (request);
    ufo_message_free (response);
}

void
ufo_remote_node_send_inputs (UfoRemoteNode *node,
                             UfoBuffer **inputs)
{
    UfoRemoteNodePrivate *priv;

    g_return_if_fail (UFO_IS_REMOTE_NODE (node));

    priv = node->priv;

    // we currently only support one input
    g_assert (priv->n_inputs == 1);

    // first, send the requisition of the input
    UfoRequisition requisition;
    ufo_buffer_get_requisition (inputs[0], &requisition);
    UfoMessage *request = ufo_message_new (UFO_MESSAGE_SEND_INPUTS_REQUISITION, 0);
    request->data = &requisition;
    request->data_size = sizeof (UfoRequisition);
    ufo_messenger_send_blocking (priv->msger, request, NULL);
    g_free (request);
    
    // second, send the input payload
    request = ufo_message_new (UFO_MESSAGE_SEND_INPUTS_DATA, 0);
    request->data_size = ufo_buffer_get_size (inputs[0]);
    request->data = ufo_buffer_get_host_array (inputs[0], NULL);
    ufo_messenger_send_blocking (priv->msger, request, NULL);
    g_free (request);

    // for (guint i = 0; i < priv->n_inputs; i++) {
    //     struct _Header *header = g_new0 (struct _Header, 1);
    //     guint64 size = sizeof (struct _Header) + ufo_buffer_get_size (inputs[i]);
    //     ufo_buffer_get_requisition (inputs[i], &header->requisition);

    //     memcpy (base, header, sizeof (struct _Header));
    //     base += sizeof (struct _Header);
    // 	TraceHandle *th = trace_start ("GET_HOSTARRAY_SEND_INPUT_BUFFER");
    //     gpointer host_array = ufo_buffer_get_host_array (inputs[i], NULL);
    // 	trace_stop (th);
    // 	th = trace_start ("MEMCPY_SEND_INPUT_BUFFER");
    //     memcpy (base, host_array, header->buffer_size);
    // 	trace_stop (th);
    //     base += header->buffer_size;
    // }
    // request = ufo_message_new (UFO_MESSAGE_SEND_INPUTS, size);
    // g_free (request->data);
    // request->data = buffer;
    // send as a single message
    // ufo_messenger_send_blocking (priv->msger, request, NULL);

}

void
ufo_remote_node_get_result (UfoRemoteNode *node,
                            UfoBuffer *buffer)
{
    UfoRemoteNodePrivate *priv;
    UfoMessage *request, *response;
    gpointer host_array;

    g_return_if_fail (UFO_IS_REMOTE_NODE (node));

    priv = node->priv;
    request = ufo_message_new (UFO_MESSAGE_GET_RESULT, 0);
    response = ufo_messenger_send_blocking (priv->msger, request, NULL);

    // ufo_buffer_discard_location (buffer);
    g_assert (ufo_buffer_get_size (buffer) == response->data_size);
    ufo_buffer_set_host_array (buffer, (gfloat *) response->data, TRUE);
    ufo_message_free (request);
    g_free (response);
}

void
ufo_remote_node_get_requisition (UfoRemoteNode *node,
                                 UfoRequisition *requisition)
{
    UfoRemoteNodePrivate *priv;
    UfoMessage *request, *response;

    g_return_if_fail (UFO_IS_REMOTE_NODE (node));

    priv = node->priv;
    request = ufo_message_new (UFO_MESSAGE_GET_REQUISITION, 0);
    response = ufo_messenger_send_blocking (priv->msger, request, NULL);

    g_assert (response->data_size == sizeof (UfoRequisition));
    memcpy (requisition, response->data, sizeof (UfoRequisition));

    ufo_message_free(request);
    ufo_message_free(response);
}

static void
cleanup_remote (UfoRemoteNodePrivate *priv)
{
    UfoMessage *request = ufo_message_new (UFO_MESSAGE_CLEANUP, 0);
    ufo_messenger_send_blocking (priv->msger, request, NULL);
    ufo_message_free (request);
}

void
ufo_remote_node_terminate (UfoRemoteNode *node)
{
    UfoRemoteNodePrivate *priv = UFO_REMOTE_NODE_GET_PRIVATE (node);

    priv->terminated = TRUE;
    cleanup_remote (priv);

    UfoMessage *request;

    g_return_if_fail (UFO_IS_REMOTE_NODE (node));

    priv = node->priv;
    request = ufo_message_new (UFO_MESSAGE_TERMINATE, 0);
    ufo_messenger_send_blocking (priv->msger, request, NULL);

    ufo_messenger_disconnect (priv->msger);
    return; 
}

static void
ufo_remote_node_dispose (GObject *object)
{
    UfoRemoteNodePrivate *priv = UFO_REMOTE_NODE_GET_PRIVATE (object);

    if (!priv->terminated) {
        cleanup_remote (priv);
        ufo_messenger_disconnect (priv->msger);
    }

    G_OBJECT_CLASS (ufo_remote_node_parent_class)->dispose (object);
}

static void
ufo_remote_node_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_remote_node_parent_class)->finalize (object);
}

static void
ufo_remote_node_class_init (UfoRemoteNodeClass *klass)
{
    GObjectClass *oclass;
    global_clock = g_timer_new ();

    oclass = G_OBJECT_CLASS (klass);
    oclass->dispose = ufo_remote_node_dispose;
    oclass->finalize = ufo_remote_node_finalize;

    g_type_class_add_private (klass, sizeof(UfoRemoteNodePrivate));
}

static void
ufo_remote_node_init (UfoRemoteNode *self)
{
    UfoRemoteNodePrivate *priv;
    self->priv = priv = UFO_REMOTE_NODE_GET_PRIVATE (self);
    priv->n_inputs = 0;
    priv->terminated = FALSE;
}
