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
    GMutex *mutex;
    UfoMessengerRole role;
};

/* C99 allows flexible length structs that we use to map
* arbitrary frame lengths that are transferred via mpi.
* Note: Sizes of datatypes should be fixed and equal on all plattforms
* (i.e. don't use a gsize as it has different size on x86 & x86_64)
*/
typedef struct _DataFrame {
    UfoMessageType type;
    guint64 data_size;
    // variable length data field
    char data[];
} DataFrame;


UfoMpiMessenger *
ufo_mpi_messenger_new (void)
{
    UfoMpiMessenger *msger;
    msger = UFO_MPI_MESSENGER (g_object_new (UFO_TYPE_MPI_MESSENGER, NULL));

    UfoMpiMessengerPrivate *priv = UFO_MPI_MESSENGER_GET_PRIVATE (msger);
    MPI_Comm_rank (MPI_COMM_WORLD, &priv->own_rank);
    MPI_Comm_size (MPI_COMM_WORLD, &priv->global_size);
    priv->pid = getpid ();
    return msger;   
}

void
ufo_mpi_messenger_connect (UfoMessenger *msger, gchar *addr, UfoMessengerRole role)
{
    UfoMpiMessengerPrivate *priv = UFO_MPI_MESSENGER_GET_PRIVATE (msger);

    int remote_rank = g_ascii_strtoll (addr, NULL, 10);
    if (role == UFO_MESSENGER_CLIENT)
        priv->remote_rank = remote_rank;
    //g_debug ("[%d:%d]: connected to: %d", priv->pid, priv->own_rank, priv->remote_rank);
}

void
ufo_mpi_messenger_disconnect (UfoMessenger *msger)
{
}

UfoMessage *
ufo_mpi_messenger_send_blocking (UfoMessenger *msger,
                                 UfoMessage *request_msg,
                                 GError **error)
{
    UfoMpiMessengerPrivate *priv = UFO_MPI_MESSENGER_GET_PRIVATE (msger);

    g_mutex_lock (priv->mutex);

    // we send in two phaess: first send the data frame of fixed size
    // then the receiver knows how much bytes will follow in the second send
    DataFrame *request_frame = g_malloc0 (sizeof (DataFrame));
    request_frame->type = request_msg->type;
    request_frame->data_size = request_msg->data_size;

    // send preflight
    //g_debug ("[%d:%d] SEND sending preflight to: %d", priv->pid, priv->own_rank, priv->remote_rank);
    MPI_Send (request_frame, sizeof (DataFrame), MPI_CHAR, priv->remote_rank, 0, MPI_COMM_WORLD);
    //g_debug ("[%d:%d] SEND preflight done to: %d", priv->pid, priv->own_rank, priv->remote_rank);

    // send payload
    if (request_msg->data_size > 0) {
        //g_debug ("[%d:%d] SEND sending payload to: %d", priv->pid, priv->own_rank, priv->remote_rank);
        MPI_Send (request_msg->data, request_msg->data_size, MPI_CHAR, priv->remote_rank, 0, MPI_COMM_WORLD);
        //g_debug ("[%d:%d] SEND payload done to: %d", priv->pid, priv->own_rank, priv->remote_rank);
    }

    if (request_msg->type == UFO_MESSAGE_ACK) {
        goto finalize;
    }
    
    // receive the response
    MPI_Status status;
    UfoMessage *response = g_malloc0 (sizeof (UfoMessage));

    // reuse the memory buffer
    DataFrame *response_frame = request_frame;
    //g_debug ("[%d:%d] SEND waiting for response preflight from: %d", priv->pid, priv->own_rank, priv->remote_rank);
    MPI_Recv (response_frame, sizeof (DataFrame), MPI_CHAR, priv->remote_rank, 0, MPI_COMM_WORLD, &status);
    //g_debug ("[%d:%d] SEND response preflight received from: %d SIZE:%d", priv->pid, priv->own_rank, priv->remote_rank, response_frame->data_size);
 
    response->type = response_frame->type;
    response->data_size = response_frame->data_size;

    if (response_frame->data_size > 0) {
        gpointer buff = g_malloc0 (response_frame->data_size);
        //g_debug ("[%d:%d] SEND waiting for response payload from: %d", priv->pid, priv->own_rank, priv->remote_rank);
        MPI_Recv (buff, response_frame->data_size, MPI_CHAR, priv->remote_rank, 0, MPI_COMM_WORLD, &status);
        //g_debug ("[%d:%d] SEND payload received from: %d", priv->pid, priv->own_rank, priv->remote_rank);
        response->data = buff;
    }

    finalize:
        g_mutex_unlock (priv->mutex);
        return response;
}

UfoMessage *
ufo_mpi_messenger_recv_blocking (UfoMessenger *msger,
                                 GError **error)
{
    UfoMpiMessengerPrivate *priv = UFO_MPI_MESSENGER_GET_PRIVATE (msger);

    g_mutex_lock (priv->mutex);
    UfoMessage *response = g_malloc0 (sizeof (DataFrame));
    DataFrame *frame = g_malloc0 (sizeof (DataFrame));
    MPI_Status status;
    
    //g_debug ("[%d:%d] RECV waiting for preflight from %d", priv->pid, priv->own_rank, priv->remote_rank);
    MPI_Recv (frame, sizeof (DataFrame), MPI_CHAR, priv->remote_rank, 0, MPI_COMM_WORLD, &status);
    //g_debug ("[%d:%d] RECV preflight received from %d, size: %d", priv->pid, priv->own_rank, priv->remote_rank, frame->data_size);
    response->type = frame->type;
    response->data_size = frame->data_size;

    if (frame->data_size > 0) {
        gpointer buff = g_malloc0 (frame->data_size);
        //g_debug ("[%d:%d] RECV waiting for payload from %d", priv->pid, priv->own_rank, priv->remote_rank);
        MPI_Recv (buff, frame->data_size, MPI_CHAR, priv->remote_rank, 0, MPI_COMM_WORLD, &status);
        //g_debug ("[%d:%d] RECV payload received from %d", priv->pid, priv->own_rank, priv->remote_rank);
        response->data = buff;
    }
    g_mutex_unlock (priv->mutex);
    return response;
}

static void
ufo_messenger_interface_init (UfoMessengerIface *iface)
{
    iface->connect = ufo_mpi_messenger_connect;
    iface->disconnect = ufo_mpi_messenger_disconnect;
    iface->send_blocking = ufo_mpi_messenger_send_blocking;
    iface->recv_blocking = ufo_mpi_messenger_recv_blocking;
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
    priv->mutex = g_mutex_new ();
}
