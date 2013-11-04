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
};

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
    g_free(addr);

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
    } msg_data;

    g_return_if_fail (UFO_IS_REMOTE_NODE (node));
    *mode = UFO_TASK_MODE_PROCESSOR;


    request = ufo_message_new (UFO_MESSAGE_GET_STRUCTURE, 0);
    response = ufo_messenger_send_blocking (priv->msger, request, NULL);
    g_assert (response->data_size == sizeof (struct _Structure));

    msg_data = *(struct _Structure *) response->data;

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
    UfoMessage *request;

    g_return_if_fail (UFO_IS_REMOTE_NODE (node));

    priv = node->priv;

    /*
     * For each of the input data items send two frames: the first one contains
     * the size as an UfoRequisition struct and the second one the raw byte
     * data.
     */
    struct _Header {
        UfoRequisition requisition;
        guint64 buffer_size;
    };

    // determine our total message size
    guint64 size = 0;

    for (guint i = 0; i < priv->n_inputs; i++) {
        guint64 buffer_size = ufo_buffer_get_size (inputs[i]);
        size += buffer_size;
    }

    gpointer buffer = g_malloc (priv->n_inputs * sizeof (struct _Header) + size);

    char *base = buffer;

    for (guint i = 0; i < priv->n_inputs; i++) {
        struct _Header *header = g_new0 (struct _Header, 1);
        ufo_buffer_get_requisition (inputs[i], &header->requisition);
        header->buffer_size = (guint64) ufo_buffer_get_size (inputs[i]);

        memcpy (base, header, sizeof (struct _Header));
        base += sizeof (struct _Header);
        memcpy (base, ufo_buffer_get_host_array (inputs[i], NULL), header->buffer_size);
        base += header->buffer_size;
    }

    request = ufo_message_new (UFO_MESSAGE_SEND_INPUTS, size);
    g_free (request->data);
    request->data = buffer;
    // send as a single message
    ufo_messenger_send_blocking (priv->msger, request, NULL);

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

    ufo_buffer_discard_location (buffer);
    host_array = ufo_buffer_get_host_array (buffer, NULL);
    g_assert (ufo_buffer_get_size (buffer) == response->data_size);

    memcpy (host_array, response->data, ufo_buffer_get_size (buffer));

    ufo_message_free (request);
    ufo_message_free (response);
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
