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

#include <zmq.h>
#include <string.h>
#include <ufo-remote-node.h>

G_DEFINE_TYPE (UfoRemoteNode, ufo_remote_node, UFO_TYPE_NODE)

#define UFO_REMOTE_NODE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_REMOTE_NODE, UfoRemoteNodePrivate))

static void ufo_msg_send (UfoMessage *msg, gpointer socket, gint flags);
static void receive_ack (gpointer socket);

struct _UfoRemoteNodePrivate {
    gpointer context;
    gpointer socket;
    guint n_inputs;
};

UfoNode *
ufo_remote_node_new (gpointer zmq_context,
                     const gchar *address)
{
    UfoRemoteNode *node;
    UfoRemoteNodePrivate *priv;

    g_return_val_if_fail (zmq_context != NULL && address != NULL, NULL);
    node = UFO_REMOTE_NODE (g_object_new (UFO_TYPE_REMOTE_NODE, NULL));
    priv = node->priv;
    priv->context = zmq_context;
    priv->socket = zmq_socket (zmq_context, ZMQ_REQ);

    if (zmq_connect (priv->socket, address) == 0) {
        g_message ("Connected remote node to `%s' via socket=%p",
                   address,
                   priv->socket);
        return UFO_NODE (node);
    }
    else {
        g_warning ("Could not connect to `%s': %s",
                   address,
                   zmq_strerror (errno));
        g_object_unref (node);
        return NULL;
    }
}

void
ufo_remote_node_request_setup (UfoRemoteNode *node)
{
    UfoRemoteNodePrivate *priv;
    UfoMessage request;

    g_return_if_fail (UFO_IS_REMOTE_NODE (node));

    priv = node->priv;
    request.type = UFO_MESSAGE_SETUP;
    ufo_msg_send (&request, priv->socket, 0);

    receive_ack (priv->socket);
}

void
ufo_remote_node_send_json (UfoRemoteNode *node,
                           const gchar *json,
                           gsize size)
{
    UfoRemoteNodePrivate *priv;
    UfoMessage request;
    zmq_msg_t json_msg;

    g_return_if_fail (UFO_IS_REMOTE_NODE (node));

    priv = node->priv;
    request.type = UFO_MESSAGE_TASK_JSON;
    ufo_msg_send (&request, priv->socket, ZMQ_SNDMORE);

    zmq_msg_init_size (&json_msg, size);
    memcpy (zmq_msg_data (&json_msg), json, size);
    zmq_msg_send (&json_msg, priv->socket, 0);
    zmq_msg_close (&json_msg);

    receive_ack (priv->socket);
}

void
ufo_remote_node_get_structure (UfoRemoteNode *node,
                               guint *n_inputs,
                               UfoInputParam **in_params,
                               UfoTaskMode *mode)
{
    UfoRemoteNodePrivate *priv;
    UfoMessage request;
    UfoMessage *header;
    zmq_msg_t header_msg;
    zmq_msg_t payload_msg;
    UfoInputParam *in_param;

    g_return_if_fail (UFO_IS_REMOTE_NODE (node));

    priv = node->priv;
    *mode = UFO_TASK_MODE_SINGLE;
    request.type = UFO_MESSAGE_GET_STRUCTURE;
    ufo_msg_send (&request, priv->socket, 0);

    /* Receive header */
    zmq_msg_init (&header_msg);
    zmq_msg_recv (&header_msg, priv->socket, 0);
    header = (UfoMessage *) zmq_msg_data (&header_msg);

    /* Receive payload */
    zmq_msg_init (&payload_msg);
    zmq_msg_recv (&payload_msg, priv->socket, 0);
    in_param = (UfoInputParam *) zmq_msg_data (&payload_msg);

    priv->n_inputs = header->n_inputs;
    *n_inputs = header->n_inputs;
    *in_params = g_new0 (UfoInputParam, 1);
    (*in_params)[0].n_dims = in_param->n_dims;
    (*in_params)[0].n_expected = in_param->n_expected;

    zmq_msg_close (&header_msg);
    zmq_msg_close (&payload_msg);
}

void
ufo_remote_node_send_inputs (UfoRemoteNode *node,
                             UfoBuffer **inputs)
{
    UfoRemoteNodePrivate *priv;
    UfoMessage request;

    g_return_if_fail (UFO_IS_REMOTE_NODE (node));

    priv = node->priv;
    request.type = UFO_MESSAGE_SEND_INPUTS;
    ufo_msg_send (&request, priv->socket, ZMQ_SNDMORE);

    /*
     * For each of the input data items send two frames: the first one contains
     * the size as an UfoRequisition struct and the second one the raw byte
     * data.
     */
    for (guint i = 0; i < priv->n_inputs; i++) {
        UfoRequisition requisition;
        zmq_msg_t requisition_msg;
        zmq_msg_t data_msg;
        gsize size;
        gint flags;

        ufo_buffer_get_requisition (inputs[i], &requisition);
        size = ufo_buffer_get_size (inputs[i]);

        zmq_msg_init_size (&requisition_msg, sizeof (UfoRequisition));
        zmq_msg_init_size (&data_msg, size);

        memcpy (zmq_msg_data (&requisition_msg), &requisition, sizeof (UfoRequisition));
        memcpy (zmq_msg_data (&data_msg), ufo_buffer_get_host_array (inputs[i], NULL), size);

        flags = i == priv->n_inputs - 1 ? 0 : ZMQ_SNDMORE;
        zmq_msg_send (&requisition_msg, priv->socket, ZMQ_SNDMORE);
        zmq_msg_send (&data_msg, priv->socket, flags);

        zmq_msg_close (&requisition_msg);
        zmq_msg_close (&data_msg);
    }

    receive_ack (priv->socket);
}

void
ufo_remote_node_get_result (UfoRemoteNode *node,
                            UfoBuffer *buffer)
{
    UfoRemoteNodePrivate *priv;
    UfoMessage request;
    zmq_msg_t reply_msg;
    gpointer host_array;

    g_return_if_fail (UFO_IS_REMOTE_NODE (node));

    priv = node->priv;
    request.type = UFO_MESSAGE_GET_RESULT;
    ufo_msg_send (&request, priv->socket, 0);

    /* Get the remote data and put it into our buffer */
    zmq_msg_init (&reply_msg);
    zmq_msg_recv (&reply_msg, priv->socket, 0);

    ufo_buffer_discard_location (buffer, UFO_LOCATION_DEVICE);
    host_array = ufo_buffer_get_host_array (buffer, NULL);
    g_assert (ufo_buffer_get_size (buffer) == zmq_msg_size (&reply_msg));
    memcpy (host_array, zmq_msg_data (&reply_msg), ufo_buffer_get_size (buffer));

    zmq_msg_close (&reply_msg);
}

void
ufo_remote_node_get_requisition (UfoRemoteNode *node,
                                 UfoRequisition *requisition)
{
    UfoRemoteNodePrivate *priv;
    UfoMessage request;
    zmq_msg_t reply_msg;

    g_return_if_fail (UFO_IS_REMOTE_NODE (node));

    priv = node->priv;
    request.type = UFO_MESSAGE_GET_REQUISITION;
    ufo_msg_send (&request, priv->socket, 0);

    zmq_msg_init (&reply_msg);
    zmq_msg_recv (&reply_msg, priv->socket, 0);
    g_assert (zmq_msg_size (&reply_msg) >= sizeof (UfoRequisition));
    memcpy (requisition, zmq_msg_data (&reply_msg), sizeof (UfoRequisition));
    zmq_msg_close (&reply_msg);
}

static void
cleanup_remote (gpointer socket)
{
    UfoMessage request;

    request.type = UFO_MESSAGE_CLEANUP;
    ufo_msg_send (&request, socket, 0);
    receive_ack (socket);
}

static void
ufo_msg_send (UfoMessage *msg,
              gpointer socket,
              gint flags)
{
    zmq_msg_t request;

    zmq_msg_init_size (&request, sizeof (UfoMessage));
    memcpy (zmq_msg_data (&request), msg, sizeof (UfoMessage));
    zmq_msg_send (&request, socket, flags);
    zmq_msg_close (&request);
}

static void
receive_ack (gpointer socket)
{
    zmq_msg_t reply_msg;

    zmq_msg_init (&reply_msg);
    zmq_msg_recv (&reply_msg, socket, 0);
    zmq_msg_close (&reply_msg);
}

static void
ufo_remote_node_dispose (GObject *object)
{
    UfoRemoteNodePrivate *priv;

    priv = UFO_REMOTE_NODE_GET_PRIVATE (object);

    if (priv->socket != NULL) {
        cleanup_remote (priv->socket);

        g_debug ("Close socket=%p", priv->socket);
        zmq_close (priv->socket);
        priv->socket = NULL;
    }

    G_OBJECT_CLASS (ufo_remote_node_parent_class)->dispose (object);
}

static void
ufo_remote_node_class_init (UfoRemoteNodeClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);
    oclass->dispose = ufo_remote_node_dispose;

    g_type_class_add_private (klass, sizeof(UfoRemoteNodePrivate));
}

static void
ufo_remote_node_init (UfoRemoteNode *self)
{
    UfoRemoteNodePrivate *priv;
    self->priv = priv = UFO_REMOTE_NODE_GET_PRIVATE (self);
    priv->context = NULL;
    priv->socket = NULL;
    priv->n_inputs = 0;
}