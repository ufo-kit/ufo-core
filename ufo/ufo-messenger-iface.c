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
        case UFO_MESSAGE_SEND_INPUTS:
            return g_strdup ("UFO_MESSAGE_SEND_INPUTS");
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
    UFO_MESSENGER_GET_IFACE (msger)->connect (msger, addr, role);
}

void
ufo_messenger_disconnect (UfoMessenger *msger)
{
    UFO_MESSENGER_GET_IFACE (msger)->disconnect (msger);
}

/**
 * ufo_messenger_send_blocking:
 * @msger: The messenger object
 * @request: (transfer none): The request #UfoMessage.
 * @error: A #GError
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
 * ufo_messenger_recv_blocking:
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
