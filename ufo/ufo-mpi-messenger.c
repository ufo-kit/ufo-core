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

#include <ufo/ufo-mpi-messenger.h>
#include <mpi.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

static void ufo_messenger_interface_init (UfoMessengerIface *iface);

#define UFO_MPI_MESSENGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_MPI_MESSENGER, UfoMpiMessengerPrivate))

G_DEFINE_TYPE_WITH_CODE (UfoMpiMessenger, ufo_mpi_messenger, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_MESSENGER,
                                                ufo_messenger_interface_init))


struct _UfoMpiMessengerPrivate {
    gint own_rank;
    gint remote_rank;
    gint pid;
    gint global_size;
    gboolean connected;
    GMutex *mutex;
    gboolean free_mutex;
    UfoMessengerRole role;
    gpointer profiler;
};

/* C99 allows flexible length structs that we use to map
* arbitrary frame lengths that are transferred via mpi.
* Note: Sizes of datatypes should be fixed and equal on all plattforms
* (i.e. don't use a gsize as it has different size on x86 & x86_64)
*/
typedef struct _DataFrame {
    UfoMessageType type;
    guint64 data_size;
    // variable length data field, goes to the stack
    char data[];
} DataFrame;

/*
 * In most MPI implementations, calls to MPI_Send/Recv are not thread safe.
 * If MPI_THREADS_MULTIPLE is supported, the mpi messenger becomes thread-safe
 * too. However, MPI_THREAD_MULTIPE is often not working with InfiniBand,
 * and performance when MPI_THREAD_MULTIPLE is often poor.
 * We are better of with MPI_THREAD__SERIALIZED and then have to lock on
 * our own. In that case, the ufo_mpi_messenger_send_blocking()
 * and _recv_blocking() are NOT thread safe! Only _connect and _disconnect
 * are thread-safe.
 */
UfoMpiMessenger *
ufo_mpi_messenger_new ()
{
    UfoMpiMessenger *msger;
    msger = UFO_MPI_MESSENGER (g_object_new (UFO_TYPE_MPI_MESSENGER, NULL));

    UfoMpiMessengerPrivate *priv = UFO_MPI_MESSENGER_GET_PRIVATE (msger);
    MPI_Comm_rank (MPI_COMM_WORLD, &priv->own_rank);
    MPI_Comm_size (MPI_COMM_WORLD, &priv->global_size);

    gint provided_thread_level;
    MPI_Query_thread (&provided_thread_level);
    if (provided_thread_level < MPI_THREAD_SERIALIZED) {
        g_critical ("No Threading support in MPI implemenation found. I need at least MPI_THREAD_SERIALIZED!");
    }
    priv->mutex = g_mutex_new ();
    priv->profiler = ufo_profiler_new ();

    return msger;
}

void
ufo_mpi_messenger_connect (UfoMessenger *msger, gchar *addr, UfoMessengerRole role)
{
    UfoMpiMessengerPrivate *priv = UFO_MPI_MESSENGER_GET_PRIVATE (msger);
    g_mutex_lock (priv->mutex);

    if (role == UFO_MESSENGER_CLIENT) {
        int remote_rank = g_ascii_strtoll (addr, NULL, 0);
        priv->remote_rank = remote_rank;
        g_debug ("[%d:%d]: CLIENT connected to: %d", priv->pid, priv->own_rank, priv->remote_rank);
    } else {
        priv->remote_rank = 0;
        g_debug ("[%d:%d]: SERVER connected to: %d", priv->pid, priv->own_rank, priv->remote_rank);
    }
    priv->connected = TRUE;
    g_mutex_unlock (priv->mutex);
}

void
ufo_mpi_messenger_disconnect (UfoMessenger *msger)
{
    UfoMpiMessengerPrivate *priv = UFO_MPI_MESSENGER_GET_PRIVATE (msger);
    g_mutex_lock (priv->mutex);
    priv->connected = FALSE;
    g_mutex_unlock (priv->mutex);
}

UfoMessage *
ufo_mpi_messenger_send_blocking (UfoMessenger *msger,
                                 UfoMessage *request_msg,
                                 GError **error)
{
    UfoMpiMessengerPrivate *priv = UFO_MPI_MESSENGER_GET_PRIVATE (msger);
    g_assert (priv->connected == TRUE);

    // we send in two phases: first send the data frame of fixed size
    // then the receiver knows how much bytes will follow in the second send
    DataFrame *request_frame = g_malloc (sizeof (DataFrame));
    request_frame->type = request_msg->type;
    request_frame->data_size = request_msg->data_size;

    // send preflight
    MPI_Ssend (request_frame, sizeof (DataFrame), MPI_CHAR, priv->remote_rank, 0, MPI_COMM_WORLD);

    // send payload
    if (request_msg->data_size > 0) {
        int err = MPI_Ssend (request_msg->data, request_msg->data_size, MPI_CHAR, priv->remote_rank, 0, MPI_COMM_WORLD);
        if (err != MPI_SUCCESS) {
            g_critical ("error on MPI_Ssend: %d", err);
            goto finalize;
        }
    }

    UfoMessage *response = g_malloc (sizeof (UfoMessage));
    response->data = NULL;
    response->data_size = 0;

    if (request_msg->type == UFO_MESSAGE_ACK) {
        goto finalize;
    }

    MPI_Status status;

    // reuse the buffer
    DataFrame *response_frame = request_frame;
    // receive the response preflight
    MPI_Recv (response_frame, sizeof (DataFrame), MPI_CHAR, priv->remote_rank, 0, MPI_COMM_WORLD, &status);

    response->type = response_frame->type;
    response->data_size = response_frame->data_size;

    // receive the response payload (if any)
    if (response_frame->data_size > 0) {
        gpointer buff = g_malloc (response_frame->data_size);
        MPI_Recv (buff, response_frame->data_size, MPI_CHAR, priv->remote_rank, 0, MPI_COMM_WORLD, &status);
        response->data = buff;
    }

    goto finalize;

    finalize:
        return response;
}

UfoMessage *
ufo_mpi_messenger_recv_blocking (UfoMessenger *msger,
                                 GError **error)
{
    UfoMpiMessengerPrivate *priv = UFO_MPI_MESSENGER_GET_PRIVATE (msger);

    g_assert (priv->connected == TRUE);

    UfoMessage *response = g_malloc (sizeof (UfoMessage));
    response->data = NULL;
    response->data_size = 0;

    DataFrame *frame = g_malloc (sizeof (DataFrame));
    MPI_Status status;

    int ret = MPI_Recv (frame, sizeof (DataFrame), MPI_CHAR, priv->remote_rank, 0, MPI_COMM_WORLD, &status);

    if (ret != MPI_SUCCESS)
        g_critical ("error on recv: %d", ret);

    response->type = frame->type;
    response->data_size = frame->data_size;

    if (frame->data_size > 0) {
        gpointer buff = g_malloc (frame->data_size);
        MPI_Recv (buff, frame->data_size, MPI_CHAR, priv->remote_rank, 0, MPI_COMM_WORLD, &status);
        response->data = buff;
    }
    return response;
}

gpointer
ufo_mpi_messenger_get_profiler (UfoMessenger *msger)
{
    UfoMpiMessengerPrivate *priv = UFO_MPI_MESSENGER_GET_PRIVATE (msger);
    return priv->profiler;
}

void
ufo_mpi_messenger_set_profiler (UfoMessenger *msger, gpointer data)
{
    UfoMpiMessengerPrivate *priv = UFO_MPI_MESSENGER_GET_PRIVATE (msger);
    priv->profiler = data;
}

static void
ufo_messenger_interface_init (UfoMessengerIface *iface)
{
    iface->connect = ufo_mpi_messenger_connect;
    iface->disconnect = ufo_mpi_messenger_disconnect;
    iface->send_blocking = ufo_mpi_messenger_send_blocking;
    iface->recv_blocking = ufo_mpi_messenger_recv_blocking;
    iface->get_profiler = ufo_mpi_messenger_get_profiler;
    iface->set_profiler = ufo_mpi_messenger_set_profiler;
}


static void
ufo_mpi_messenger_dispose (GObject *object)
{
    ufo_mpi_messenger_disconnect (UFO_MESSENGER (object));
}

static void
ufo_mpi_messenger_finalize (GObject *object)
{
    UfoMpiMessengerPrivate *priv = UFO_MPI_MESSENGER_GET_PRIVATE (object);

    g_mutex_free (priv->mutex);
}

static void
ufo_mpi_messenger_class_init (UfoMpiMessengerClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);
    oclass->dispose      = ufo_mpi_messenger_dispose;
    oclass->finalize     = ufo_mpi_messenger_finalize;

    g_type_class_add_private (klass, sizeof(UfoMpiMessengerPrivate));
}

static void
ufo_mpi_messenger_init (UfoMpiMessenger *msger)
{
    UfoMpiMessengerPrivate *priv = UFO_MPI_MESSENGER_GET_PRIVATE (msger);
    priv->connected = FALSE;
}
