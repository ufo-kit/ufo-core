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

#ifndef __UFO_MPI_MESSENGER_H
#define __UFO_MPI_MESSENGER_H

#include <ufo/ufo-remote-node.h>
#include <ufo/ufo-messenger-iface.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_MPI_MESSENGER             (ufo_mpi_messenger_get_type())
#define UFO_MPI_MESSENGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_MPI_MESSENGER, UfoMpiMessenger))
#define UFO_IS_MPI_MESSENGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_MPI_MESSENGER))
#define UFO_MPI_MESSENGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_MPI_MESSENGER, UfoMpiMessengerClass))
#define UFO_IS_MPI_MESSENGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_MPI_MESSENGER))
#define UFO_MPI_MESSENGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_MPI_MESSENGER, UfoMpiMessengerClass))

typedef struct _UfoMpiMessenger           UfoMpiMessenger;
typedef struct _UfoMpiMessengerClass      UfoMpiMessengerClass;
typedef struct _UfoMpiMessengerPrivate    UfoMpiMessengerPrivate;

struct _UfoMpiMessenger {
    /*< private >*/
    GObject parent_instance;

    UfoMpiMessengerPrivate *priv;
};

struct _UfoMpiMessengerClass {
    /*< private >*/
    GObjectClass parent_class;
};

UfoMpiMessenger    *ufo_mpi_messenger_new           (void);
GType               ufo_mpi_messenger_get_type      (void);

void                ufo_mpi_messenger_connect       (UfoMessenger *msger,
                                                     gchar *addr,
                                                     UfoMessengerRole role);

void                ufo_mpi_messenger_disconnect    (UfoMessenger *msg);

UfoMessage         *ufo_mpi_messenger_send_blocking (UfoMessenger *msger,
                                                     UfoMessage *request,
                                                     GError **error);

gpointer            ufo_mpi_messenger_get_profiler  (UfoMessenger *msg);
void                ufo_mpi_messenger_set_profiler  (UfoMessenger *msg, gpointer data);
UfoMessage         *ufo_mpi_messenger_recv_blocking (UfoMessenger *msger,
                                                     GError **error);

G_END_DECLS

#endif
