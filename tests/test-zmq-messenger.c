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

#include <ufo/ufo.h>
#include <ufo/ufo-zmq-messenger.h>
#include "test-suite.h"

typedef struct {
    gchar *addr;
} Fixture;

static void
setup (Fixture *fixture, gconstpointer data)
{
    fixture->addr = g_strdup ("tcp://127.0.0.1:5555");
}

static void
teardown (Fixture *fixture, gconstpointer data)
{
    g_free (fixture->addr);
}

static void send_num_devices_request (gpointer unused)
{
    UfoMessenger *msger = UFO_MESSENGER (ufo_zmq_messenger_new ());
    gchar *addr = g_strdup ("tcp://127.0.0.1:5555");
    ufo_messenger_connect (msger, addr, UFO_MESSENGER_CLIENT);

    guint x = 0;
    while (x++ < 10) {
        UfoMessage *request = ufo_message_new (UFO_MESSAGE_GET_NUM_DEVICES, 0);
        UfoMessage *response;

        response = ufo_messenger_send_blocking (msger, request, NULL);

        guint16 num_devices = *(guint16 *) response->data;
        g_assert (num_devices == x);

        ufo_message_free (request);
        ufo_message_free (response);
    }
    ufo_zmq_messenger_disconnect (msger);
    g_object_unref (msger);
}

static void handle_num_devices (gpointer unused)
{
    UfoMessenger *msger = UFO_MESSENGER (ufo_zmq_messenger_new ());
    gchar *addr = g_strdup ("tcp://127.0.0.1:5555");
    ufo_messenger_connect (msger, addr, UFO_MESSENGER_SERVER);

    guint16 x = 0;
    GError *err = NULL;
    while (x++ < 10) {
        UfoMessage *msg = ufo_messenger_recv_blocking (UFO_MESSENGER (msger), &err);
        if (err != NULL)
            g_critical ("%s", err->message);

        UfoMessage *resp;
        switch (msg->type) {
            case UFO_MESSAGE_GET_NUM_DEVICES:
                resp = ufo_message_new (UFO_MESSAGE_ACK, sizeof (guint16));
                *(guint16 *)resp->data = x;
                ufo_zmq_messenger_send_blocking (msger, resp, NULL);
                ufo_message_free (resp);
                break;
            default:
                g_critical ("Unexpected message type: %d", msg->type);
            break;
        }
        ufo_message_free (msg);
    };

    ufo_zmq_messenger_disconnect (msger);
    g_object_unref (msger);
}

static void test_zmq_messenger (Fixture *fixture, gconstpointer unused)
{
    GThread *server = g_thread_create ((GThreadFunc) handle_num_devices, NULL, TRUE, NULL);
    GThread *client = g_thread_create ((GThreadFunc) send_num_devices_request, NULL, TRUE, NULL);

    g_thread_join (client);
    g_thread_join (server);
}


void
test_add_zmq_messenger (void)
{
    g_test_add ("/opencl/zmq_messenger/test_messenger",
                Fixture, NULL,
                setup, test_zmq_messenger, teardown);
}
