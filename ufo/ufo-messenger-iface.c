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

#include <config.h>

#ifdef WITH_ZMQ
#include <ufo/ufo-zmq-messenger.h>
#endif

#ifdef WITH_MPI
#include <ufo/ufo-mpi-messenger.h>
#endif

#ifdef WITH_KIRO
#include <ufo/ufo-kiro-messenger.h>
#endif

#include <ufo/ufo-messenger-iface.h>

typedef UfoMessengerIface UfoMessengerInterface;


/**
 * ufo_messenger_create:
 * @address: (transfer none) (type utf8): listen address for the messenger
 * @error: A #GError used to report errors during messenger creation
 *
 * Create a new #UfoMessenger basend on the PROTOCOL:// of the given @address
 *
 * Returns: (transfer full) (allow-none): A new #UfoMessenger or %NULL in case of
 * error.
 */
UfoMessenger *
ufo_messenger_create (const gchar *address, GError **error)
{
    UfoMessenger *msgr_out = NULL;
    GError *error_internal = NULL;
    GRegex *regex = g_regex_new ("^[a-z A-Z]+://[a-z A-Z 0-9 \\.]+:[0-9]{1,5}", \
                                 0, G_REGEX_MATCH_NOTEMPTY, &error_internal);
    if (error_internal) {
        g_propagate_error (error, error_internal);
        return NULL;
    }
    if (g_regex_match_all (regex, address, 0, NULL)) {
        gchar **protocol = g_strsplit (address, ":", 2);
        g_debug ("Trying to kreate a new messenger for the '%s://' protocol", protocol[0]);

#ifdef WITH_ZMQ
        if (!g_strcmp0 (protocol[0], "tcp")) {
            msgr_out = UFO_MESSENGER (ufo_zmq_messenger_new ());
            goto done;
        }
#endif

#ifdef WITH_MPI
        if (!g_strcmp0 (protocol[0], "mpi")) {
            msgr_out = UFO_MESSENGER (ufo_mpi_messenger_new ());
            goto done;
        }
#endif

#ifdef WITH_KIRO
        if (!g_strcmp0 (protocol[0], "kiro")) {
            msgr_out = UFO_MESSENGER (ufo_kiro_messenger_new ());
            goto done;
        }
#endif

        g_set_error (error,
                     UFO_MESSENGER_ERROR,
                     UFO_MESSENGER_UNKNOWN_PROTOCOL,
                     "Don't know how to handle protocol '%s://'", protocol[0]);
done:
        g_strfreev (protocol);
    }
    else {
        g_set_error (error,
                     UFO_MESSENGER_ERROR,
                     UFO_MESSENGER_INVALID_ADDRESS,
                     "The given address has invalid format. (Expecting \"<protocol>://<address | device>:<port>\")");
    }

    g_regex_unref (regex);
    return msgr_out;
}

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
 * @UFO_MESSENGER_INVALID_ADDRESS: Given listen address is invalid
 * @UFO_MESSENGER_UNKNOWN_PROTOCOL: The given address is of unknown PROTOCOL://
 */
GQuark
ufo_messenger_error_quark ()
{
    return g_quark_from_static_string ("ufo-messenger-error-quark");
}

/**
 * ufo_messenger_connect:
 * @messenger: The messenger object
 * @addr: (transfer none) : The address to connect. This is implementation specific.
 * @role: The role of the local endpoint (client or server).
 * @error: (allow-none): Location for a #GError or %NULL.
 *
 * Connects a messenger to and endpoint.
 */
void
ufo_messenger_connect (UfoMessenger *messenger,
                       const gchar *addr,
                       UfoMessengerRole role,
                       GError **error)
{
    UFO_MESSENGER_GET_IFACE (messenger)->connect (messenger, addr, role, error);
}

void
ufo_messenger_disconnect (UfoMessenger *messenger)
{
    UFO_MESSENGER_GET_IFACE (messenger)->disconnect (messenger);
}

/**
 * ufo_messenger_send_blocking: (skip)
 * @messenger: The messenger object
 * @request: (transfer none): The request #UfoMessage.
 * @error: A #GError
 *
 * Returns: (allow-none) : A #UfoMessage response to the sent request.
 *
 * Sends a #UfoMessage request to the connected
 * endpoint and blocks until the message want fully sent.
 */
UfoMessage *
ufo_messenger_send_blocking (UfoMessenger *messenger,
                             UfoMessage *request,
                             GError **error)
{
    return UFO_MESSENGER_GET_IFACE (messenger)->send_blocking (messenger, request, error);
}

/**
 * ufo_messenger_recv_blocking: (skip)
 * @messenger: The messenger object.
 * @error: The #GError object
 *
 * Returns: The received #UfoMessage.
 *
 * Receives a #UfoMessage from the connected endpoint and blocks until the
 * message was fully received.
 *
 */
UfoMessage *
ufo_messenger_recv_blocking (UfoMessenger *messenger,
                            GError **error)
{
    return UFO_MESSENGER_GET_IFACE (messenger)->recv_blocking (messenger, error);
}

static void
ufo_messenger_default_init (UfoMessengerInterface *iface)
{
}
