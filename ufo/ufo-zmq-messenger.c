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

#include <ufo/ufo-zmq-messenger.h>
#include <ufo/ufo-messenger-iface.h>
#include <zmq.h>
#include <string.h>

#include "zmq-shim.h"

static void ufo_messenger_interface_init (UfoMessengerIface *iface);

#define UFO_ZMQ_MESSENGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_ZMQ_MESSENGER, UfoZmqMessengerPrivate))

G_DEFINE_TYPE_WITH_CODE (UfoZmqMessenger, ufo_zmq_messenger, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_MESSENGER,
                                                ufo_messenger_interface_init))

struct _UfoZmqMessengerPrivate {
    gchar *remote_addr;
    GMutex *mutex;
    gpointer zmq_socket;
    gpointer zmq_ctx;
    UfoMessengerRole role;
    gpointer profiler;
};

/* C99 allows flexible length structs that we use to map
* arbitrary frame lengths that are transferred via zmq.
* Note: Sizes of datatypes should be fixed and equal on all plattforms
* (i.e. don't use a gsize as it has different size on x86 & x86_64)
*/
typedef struct _DataFrame {
    UfoMessageType type;
    guint64 data_size;
    // variable length data field
    char data[];
} DataFrame;

UfoZmqMessenger *
ufo_zmq_messenger_new (void)
{
    UfoZmqMessenger *msger;
    msger = UFO_ZMQ_MESSENGER (g_object_new (UFO_TYPE_ZMQ_MESSENGER, NULL));

    UfoZmqMessengerPrivate *priv = UFO_ZMQ_MESSENGER_GET_PRIVATE (msger);
    priv->zmq_ctx = zmq_ctx_new ();

    return msger;
}

static void
validate_zmq_listen_address (gchar *addr)
{
    if (g_str_has_prefix (addr, "ipc://"))
        return;

    if (!g_str_has_prefix (addr, "tcp://"))
        g_critical ("address didn't start with tcp:// scheme, which is required currently");

    /* Pitfall: zmq will silently accept hostnames like tcp://localhost:5555
     * but not bind to it as it treats it like an interface name (like eth0).
     * We have to use IP addresses instead of DNS names.
     */
    gchar *host = g_strdup (&addr[6]);
    if (!g_ascii_isdigit (host[0]) && host[0] != '*')
        g_message ("treating address %s as interface device name. Use IP address if supplying a host was intended.", host);
    g_free (host);
}

void
ufo_zmq_messenger_connect (UfoMessenger *msger, gchar *addr, UfoMessengerRole role)
{
    UfoZmqMessengerPrivate *priv = UFO_ZMQ_MESSENGER_GET_PRIVATE (msger);

    priv->remote_addr = g_strdup (addr);
    priv->role = role;

    if (role == UFO_MESSENGER_CLIENT) {
        priv->zmq_socket = zmq_socket (priv->zmq_ctx, ZMQ_REQ);

        if (zmq_connect (priv->zmq_socket, priv->remote_addr) == 0) {
            g_message ("Connected to `%s' via socket=%p",
                       priv->remote_addr,
                       priv->zmq_socket);
        }
        else {
            g_warning ("Could not connect to `%s': %s",
                        addr,
                        zmq_strerror (errno));
        }
    } else if (role == UFO_MESSENGER_SERVER) {
        validate_zmq_listen_address (priv->remote_addr);
        priv->zmq_socket = zmq_socket (priv->zmq_ctx, ZMQ_REP);

        gint err = zmq_bind (priv->zmq_socket, priv->remote_addr);
        if (err < 0)
            g_critical ("could not bind to address %s", priv->remote_addr);
    }

    return;
}

void
ufo_zmq_messenger_disconnect (UfoMessenger *msger)
{
    UfoZmqMessengerPrivate *priv = UFO_ZMQ_MESSENGER_GET_PRIVATE (msger);

    if (priv->zmq_socket != NULL) {
        zmq_close (priv->zmq_socket);
        priv->zmq_socket = NULL;

        // waits for outstanding messages to be flushed
        zmq_term (priv->zmq_ctx);
        g_free (priv->remote_addr);
    }

    return;
}

/**
 * ufo_zmq_messenger_send_blocking: (skip)
 * @msger: #UfoMessenger
 * @request_msg: Request message
 * @error: Location for an error or %NULL
 *
 * Send message in blocking way.
 */
UfoMessage *
ufo_zmq_messenger_send_blocking (UfoMessenger *msger,
                                 UfoMessage *request_msg,
                                 GError **error)
{
    UfoZmqMessengerPrivate *priv = UFO_ZMQ_MESSENGER_GET_PRIVATE (msger);

    if (request_msg->type == UFO_MESSAGE_ACK && priv->role == UFO_MESSENGER_CLIENT)
        g_critical ("Clients can't send ACK messages");

    UfoMessage *result = NULL;
    zmq_msg_t request;

    gsize frame_size = sizeof (DataFrame) + request_msg->data_size;
    zmq_msg_init_size (&request, frame_size);
    DataFrame *frame = (DataFrame *) zmq_msg_data (&request);

    frame->data_size = request_msg->data_size;
    frame->type = request_msg->type;
    //TODO eliminate extra copying
    memcpy (frame->data, request_msg->data, request_msg->data_size);

    gint err = zmq_msg_send (&request, priv->zmq_socket, 0);
    zmq_msg_close (&request);

    if (err < 0) {
        g_set_error (error, ufo_messenger_error_quark (), zmq_errno (),
                     "Error sending message via %s: %s",
                     priv->remote_addr, zmq_strerror (zmq_errno ()));
        goto finalize;
    }

    /* if this is an ACK message, don't expect a response
     * (send_blocking is then most likely being called by the server)
     */
    if (request_msg->type == UFO_MESSAGE_ACK) {
        goto finalize;
    }

    /* we always need to receive as response as ZMQ
     * requires REQ/REP/REQ/REP/... scheme
     */
    zmq_msg_t reply;
    zmq_msg_init (&reply);

    err = zmq_msg_recv (&reply, priv->zmq_socket, 0);
    gint size = zmq_msg_size (&reply);
    if (err < 0) {
        g_set_error (error, ufo_messenger_error_quark (), zmq_errno(),
                     "Could not receive from %s: %s ", priv->remote_addr,
                     zmq_strerror (zmq_errno ()));
        goto finalize;
    }

    DataFrame *resp_frame = (DataFrame *) zmq_msg_data (&reply);

    guint64 expected_size = (guint32) (sizeof (DataFrame) + resp_frame->data_size);
    if ((guint64) size != expected_size) {
        g_set_error (error, ufo_messenger_error_quark(),
                     UFO_MESSENGER_SIZE_MISSMATCH,
                     "Received unexpected frame size: %d", size);
        goto finalize;
    }

    UfoMessage *reply_msg = ufo_message_new (resp_frame->type, resp_frame->data_size);
    memcpy (reply_msg->data, resp_frame->data, resp_frame->data_size);

    zmq_msg_close (&reply);
    result = reply_msg;
    goto finalize;

    finalize:
        return result;

}

/**
 * ufo_zmq_messenger_recv_blocking: (skip)
 * @msger: A #UfoMessenger
 * @error: Location for an error or %NULL
 *
 * Receive message in blocking way.
 */
UfoMessage *
ufo_zmq_messenger_recv_blocking (UfoMessenger *msger,
                                 GError **error)
{
    UfoZmqMessengerPrivate *priv = UFO_ZMQ_MESSENGER_GET_PRIVATE (msger);
    g_assert (priv->role == UFO_MESSENGER_SERVER);

    UfoMessage *result = NULL;
    zmq_msg_t reply;
    zmq_msg_init (&reply);
    gint err = zmq_msg_recv (&reply, priv->zmq_socket, 0);
    gint size = zmq_msg_size (&reply);

    if (err < 0) {
        zmq_msg_close (&reply);
        g_set_error (error, ufo_messenger_error_quark(), zmq_errno(),
                     "Could not receive from %s: %s ", priv->remote_addr,
                     zmq_strerror (zmq_errno ()));
        goto finalize;
    }

    DataFrame *frame = zmq_msg_data (&reply);

    guint expected_size = (guint) (sizeof (DataFrame) + frame->data_size);
    if ((guint)size != expected_size) {
        g_set_error (error, ufo_messenger_error_quark(),
                     UFO_MESSENGER_SIZE_MISSMATCH,
                     "Received unexpected frame size: %d, should be: %d",
                     size, expected_size);
        goto finalize;
    }

    UfoMessage *msg = ufo_message_new (frame->type, frame->data_size);
    memcpy (msg->data, frame->data, frame->data_size);

    zmq_msg_close (&reply);
    result = msg;
    goto finalize;

    finalize:
        return result;
}

gpointer
ufo_zmq_messenger_get_profiler (UfoMessenger *msger)
{
    UfoZmqMessengerPrivate *priv = UFO_ZMQ_MESSENGER_GET_PRIVATE (msger);
    return priv->profiler;
}

void
ufo_zmq_messenger_set_profiler (UfoMessenger *msger, gpointer data)
{
    UfoZmqMessengerPrivate *priv = UFO_ZMQ_MESSENGER_GET_PRIVATE (msger);
    priv->profiler = data;
}

static void
ufo_messenger_interface_init (UfoMessengerIface *iface)
{
    iface->connect = ufo_zmq_messenger_connect;
    iface->disconnect = ufo_zmq_messenger_disconnect;
    iface->send_blocking = ufo_zmq_messenger_send_blocking;
    iface->recv_blocking = ufo_zmq_messenger_recv_blocking;
    iface->get_profiler = ufo_zmq_messenger_get_profiler;
    iface->set_profiler = ufo_zmq_messenger_set_profiler;
}


static void
ufo_zmq_messenger_dispose (GObject *object)
{
    ufo_zmq_messenger_disconnect (UFO_MESSENGER (object));
}

static void
ufo_zmq_messenger_finalize (GObject *object)
{
    UfoZmqMessengerPrivate *priv = UFO_ZMQ_MESSENGER_GET_PRIVATE (object);

    if  (priv->zmq_ctx != NULL) {
        zmq_ctx_destroy (priv->zmq_ctx);
        priv->zmq_ctx = NULL;
    }

}

static void
ufo_zmq_messenger_class_init (UfoZmqMessengerClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);
    oclass->dispose      = ufo_zmq_messenger_dispose;
    oclass->finalize     = ufo_zmq_messenger_finalize;

    g_type_class_add_private (klass, sizeof(UfoZmqMessengerPrivate));
}

static void
ufo_zmq_messenger_init (UfoZmqMessenger *msger)
{
    UfoZmqMessengerPrivate *priv = UFO_ZMQ_MESSENGER_GET_PRIVATE (msger);
    priv->zmq_socket = NULL;
    priv->zmq_ctx = NULL;
}
