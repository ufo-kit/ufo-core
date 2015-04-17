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

typedef UfoMessengerIface UfoMessengerInterface;

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
    UfoMessage *msg = g_malloc (sizeof (UfoMessage));
    msg->type = type;
    msg->data_size = data_size;

    if (data_size == 0)
        msg->data = NULL;
    else
        msg->data = g_malloc (data_size);
    return msg;
}

G_DEFINE_INTERFACE (UfoMessenger, ufo_messenger, G_TYPE_OBJECT)

/**
 * UfoMessengerError:
 * @UFO_MESSENGER_CONNECTION_PROBLEM: Could not establish a connection
 * @UFO_MESSENGER_BUFFER_FULL: Buffer is filled up completely
 * @UFO_MESSENGER_SIZE_MISSMATCH: Size mismatch
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
 * @error: (allow-none): Location for a #GError or %NULL.
 *
 * Connects a messenger to and endpoint.
 */
void
ufo_messenger_connect (UfoMessenger *msger,
                       const gchar *addr,
                       UfoMessengerRole role,
                       GError **error)
{
    UFO_MESSENGER_GET_IFACE (msger)->connect (msger, addr, role, error);
}

void
ufo_messenger_disconnect (UfoMessenger *msger)
{
    UFO_MESSENGER_GET_IFACE (msger)->disconnect (msger);
}

/**
 * ufo_messenger_send_blocking: (skip)
 * @msger: The messenger object
 * @request: (transfer none): The request #UfoMessage.
 * @error: A #GError
 *
 * Returns: (allow-none) : A #UfoMessage response to the sent request.
 *
 * Sends a #UfoMessage request to the connected
 * endpoint and blocks until the message want fully sent.
 */
UfoMessage *
ufo_messenger_send_blocking (UfoMessenger *msger,
                             UfoMessage *request,
                             GError **error)
{
    return UFO_MESSENGER_GET_IFACE (msger)->send_blocking (msger, request, error);
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
    return UFO_MESSENGER_GET_IFACE (msger)->recv_blocking (msger, error);
}

static void
ufo_messenger_default_init (UfoMessengerInterface *iface)
{
}
