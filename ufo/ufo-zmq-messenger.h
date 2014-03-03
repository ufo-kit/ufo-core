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

#ifndef __UFO_ZMQ_MESSENGER_H
#define __UFO_ZMQ_MESSENGER_H

#include <ufo/ufo-remote-node.h>
#include <ufo/ufo-messenger-iface.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define UFO_TYPE_ZMQ_MESSENGER             (ufo_zmq_messenger_get_type())
#define UFO_ZMQ_MESSENGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_ZMQ_MESSENGER, UfoZmqMessenger))
#define UFO_IS_ZMQ_MESSENGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_ZMQ_MESSENGER))
#define UFO_ZMQ_MESSENGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_ZMQ_MESSENGER, UfoZmqMessengerClass))
#define UFO_IS_ZMQ_MESSENGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_ZMQ_MESSENGER))
#define UFO_ZMQ_MESSENGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_ZMQ_MESSENGER, UfoZmqMessengerClass))

typedef struct _UfoZmqMessenger           UfoZmqMessenger;
typedef struct _UfoZmqMessengerClass      UfoZmqMessengerClass;
typedef struct _UfoZmqMessengerPrivate    UfoZmqMessengerPrivate;

struct _UfoZmqMessenger {
    /*< private >*/
    GObject parent_instance;

    UfoZmqMessengerPrivate *priv;
};

struct _UfoZmqMessengerClass {
    /*< private >*/
    GObjectClass parent_class;
};

UfoZmqMessenger    *ufo_zmq_messenger_new           (void);
GType               ufo_zmq_messenger_get_type      (void);

void                ufo_zmq_messenger_connect       (UfoMessenger *msger,
                                                     gchar *addr,
                                                     UfoMessengerRole role);

void                ufo_zmq_messenger_disconnect    (UfoMessenger *msg);

UfoMessage         *ufo_zmq_messenger_send_blocking (UfoMessenger *msger,
                                                     UfoMessage *request_msg,
                                                     GError **error);

UfoMessage         *ufo_zmq_messenger_recv_blocking (UfoMessenger *msger,
                                                     GError **error);
gpointer            ufo_zmq_messenger_get_profiler  (UfoMessenger *msg);
void                ufo_zmq_messenger_set_profiler  (UfoMessenger *msg, gpointer data);
G_END_DECLS

#endif
