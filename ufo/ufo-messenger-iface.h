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

#ifndef UFO_MESSENGER_H
#define UFO_MESSENGER_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <ufo/ufo-remote-node.h>

G_BEGIN_DECLS

#define UFO_TYPE_MESSENGER             (ufo_messenger_get_type())
#define UFO_MESSENGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_MESSENGER, UfoMessenger))
#define UFO_MESSENGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_MESSENGER, UfoMessengerIface))
#define UFO_IS_MESSENGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_MESSENGER))
#define UFO_IS_MESSENGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_MESSENGER))
#define UFO_MESSENGER_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE((inst), UFO_TYPE_MESSENGER, UfoMessengerIface))

#define UFO_MESSENGER_ERROR            ufo_messenger_error_quark()

typedef struct _UfoMessenger         UfoMessenger;
typedef struct _UfoMessengerIface    UfoMessengerIface;
typedef struct _UfoMessage           UfoMessage;


/**
 * UfoMessageType: (skip)
 * @UFO_MESSAGE_STREAM_JSON: insert
 * @UFO_MESSAGE_REPLICATE_JSON: insert
 * @UFO_MESSAGE_GET_NUM_DEVICES: insert
 * @UFO_MESSAGE_SETUP: insert
 * @UFO_MESSAGE_GET_STRUCTURE: insert
 * @UFO_MESSAGE_STRUCTURE: insert
 * @UFO_MESSAGE_GET_REQUISITION: insert
 * @UFO_MESSAGE_REQUISITION: insert
 * @UFO_MESSAGE_SEND_INPUTS: insert
 * @UFO_MESSAGE_GET_RESULT: insert
 * @UFO_MESSAGE_RESULT: insert
 * @UFO_MESSAGE_CLEANUP: insert
 * @UFO_MESSAGE_ACK: insert
 */
typedef enum {
    UFO_MESSAGE_STREAM_JSON = 0,
    UFO_MESSAGE_REPLICATE_JSON,
    UFO_MESSAGE_GET_NUM_DEVICES,
    UFO_MESSAGE_GET_STRUCTURE,
    UFO_MESSAGE_STRUCTURE,
    UFO_MESSAGE_GET_REQUISITION,
    UFO_MESSAGE_REQUISITION,
    UFO_MESSAGE_SEND_INPUTS,
    UFO_MESSAGE_GET_RESULT,
    UFO_MESSAGE_RESULT,
    UFO_MESSAGE_CLEANUP,
    UFO_MESSAGE_TERMINATE,
    UFO_MESSAGE_ACK
} UfoMessageType;

/**
 * UfoMessage: (skip)
 * @type: Type of the wire message
 */
struct _UfoMessage {
    UfoMessageType  type;
    guint64 data_size;
    gpointer data;
};

void ufo_message_free (UfoMessage *msg);
UfoMessage * ufo_message_new (UfoMessageType type, guint64 data_size);

typedef enum {
    UFO_MESSENGER_BUFFER_FULL,
    UFO_MESSENGER_SIZE_MISSMATCH
} UfoMessengerError;

typedef enum {
    UFO_MESSENGER_CLIENT,
    UFO_MESSENGER_SERVER
} UfoMessengerRole;

struct _UfoMessengerIface {
    /*< private >*/
    GTypeInterface parent_iface;

    void (*connect)                         (UfoMessenger   *msger,
                                             gchar *addr,
                                             UfoMessengerRole role);

    void (*disconnect)                      (UfoMessenger   *msger);

    UfoMessage * (*send_blocking)           (UfoMessenger *msger,
                                             UfoMessage *request,
                                             GError **error);

    UfoMessage * (*recv_blocking)           (UfoMessenger *msger,
                                             GError **error);
};


void        ufo_messenger_connect           (UfoMessenger *msger,
                                             gchar *addr,
                                             UfoMessengerRole role);

void        ufo_messenger_disconnect        (UfoMessenger     *msger);

UfoMessage *ufo_messenger_send_blocking     (UfoMessenger     *msger,
                                             UfoMessage *request,
                                             GError          **error);

UfoMessage *ufo_messenger_recv_blocking     (UfoMessenger     *msger,
                                             GError          **error);

GQuark      ufo_messenger_error_quark       (void);
GType       ufo_messenger_get_type          (void);

G_END_DECLS

#endif
