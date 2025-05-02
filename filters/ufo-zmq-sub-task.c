/*
 * Copyright (C) 2011-2015 Karlsruhe Institute of Technology
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
#include "config.h"

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <string.h>
#include <zmq.h>
#include <json-glib/json-glib.h>
#include "ufo-zmq-sub-task.h"
#include "ufo-zmq-common.h"


struct _UfoZmqSubTaskPrivate {
    gint32 id;
    gpointer context;
    gpointer socket;
    gchar *address;
    gboolean stop;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoZmqSubTask, ufo_zmq_sub_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_ZMQ_SUB_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_ZMQ_SUB_TASK, UfoZmqSubTaskPrivate))

enum {
    PROP_0,
    PROP_ADDRESS,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_zmq_sub_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_ZMQ_SUB_TASK, NULL));
}

static void
ufo_zmq_sub_task_setup (UfoTask *task,
                        UfoResources *resources,
                        GError **error)
{
    UfoZmqSubTaskPrivate *priv;
    gchar *addr;
    zmq_msg_t request_msg;
    zmq_msg_t reply_msg;
    ZmqRequest *request;
    ZmqReply *reply;

    priv = UFO_ZMQ_SUB_TASK_GET_PRIVATE (task);
    priv->context = zmq_ctx_new ();

    if (priv->context == NULL) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "zmq context creation failed: %s\n", zmq_strerror (zmq_errno ()));
        return;
    }

    priv->socket = zmq_socket (priv->context, ZMQ_REQ);

    if (priv->socket == NULL) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "zmq sub_socket creation failed: %s\n", zmq_strerror (zmq_errno ()));
        return;
    }

    addr = g_strdup_printf ("%s:5555", priv->address);

    if (zmq_connect (priv->socket, addr) != 0) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "zmq connect failed: %s\n", zmq_strerror (zmq_errno ()));
        g_free (addr);
        return;
    }

    g_free (addr);

    zmq_msg_init_size (&request_msg, sizeof (ZmqRequest));

    request = zmq_msg_data (&request_msg);
    /* FIXME: use a better scheme than that */
    request->id = priv->id;
    request->type = ZMQ_REQUEST_REGISTER;

    if (zmq_msg_send (&request_msg, priv->socket, 0) < 0) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "zmq msg_send failed: %s\n", zmq_strerror (zmq_errno ()));
        return;
    }

    zmq_msg_close (&request_msg);

    zmq_msg_init_size (&reply_msg, sizeof (ZmqReply));
    zmq_msg_recv (&reply_msg, priv->socket, 0);
    reply = zmq_msg_data (&reply_msg);
    g_assert (reply->type == ZMQ_REPLY_ACK && reply->error == ZMQ_ERROR_OKAY);
    zmq_msg_close (&reply_msg);
}

static gboolean
request_data (UfoZmqSubTaskPrivate *priv)
{
    zmq_msg_t request_msg;
    zmq_msg_t reply_msg;
    ZmqRequest *request;
    ZmqReply *reply;

    while (1) {
        zmq_msg_init_size (&request_msg, sizeof (ZmqRequest));
        request = zmq_msg_data (&request_msg);
        request->id = priv->id;
        request->type = ZMQ_REQUEST_DATA;
        g_assert (zmq_msg_send (&request_msg, priv->socket, 0) > 0);
        zmq_msg_close (&request_msg);

        zmq_msg_init_size (&reply_msg, sizeof (ZmqReply));
        zmq_msg_recv (&reply_msg, priv->socket, 0);
        reply = zmq_msg_data (&reply_msg);

        if (reply->error == ZMQ_ERROR_REGISTRATION_EXPECTED) {
            /*
             * We are supposed to wait until all subscribers have connected to
             * the publisher.
             */

            zmq_msg_close (&reply_msg);
            g_usleep (1000);
        }
        else if (reply->error != ZMQ_ERROR_OKAY) {
            g_warning ("Could not receive data: %i\n", reply->error);
            zmq_msg_close (&reply_msg);
            return FALSE;
        }
        else
            break;
    }

    if (reply->type == ZMQ_REPLY_STOP)
        priv->stop = TRUE;

    zmq_msg_close (&reply_msg);
    return TRUE;
}

static void
ufo_zmq_sub_task_get_requisition (UfoTask *task,
                                  UfoBuffer **inputs,
                                  UfoRequisition *requisition,
                                  GError **error)
{
    UfoZmqSubTaskPrivate *priv;
    zmq_msg_t htype_msg;
    gchar *header;
    JsonParser *parser;
    JsonObject *object;
    JsonArray *array;

    priv = UFO_ZMQ_SUB_TASK_GET_PRIVATE (task);

    if (!request_data (priv) || priv->stop)
        return;

    zmq_msg_init (&htype_msg);
    zmq_msg_recv (&htype_msg, priv->socket, 0);
    header = zmq_msg_data (&htype_msg);
    parser = json_parser_new_immutable ();

    if (!json_parser_load_from_data (parser, header, zmq_msg_size (&htype_msg), error)) {
        g_object_unref (parser);
        return;
    }

    object = json_node_get_object (json_parser_get_root (parser));
    array = json_object_get_array_member (object, "shape");
    requisition->n_dims = json_array_get_length (array);

    /* FIXME: we should get this from a public ufo-core header */
    g_assert (requisition->n_dims <= ZMQ_MAX_DIMENSIONS);

    for (guint i = 0; i < requisition->n_dims; i++) {
        requisition->dims[requisition->n_dims - 1 - i] = json_array_get_int_element (array, i);
        g_assert (requisition->dims[requisition->n_dims - 1 - i] <= ZMQ_MAX_DIMENSION_LENGTH);
    }

    zmq_msg_close (&htype_msg);
    g_object_unref (parser);
}

static guint
ufo_zmq_sub_task_get_num_inputs (UfoTask *task)
{
    return 0;
}

static guint
ufo_zmq_sub_task_get_num_dimensions (UfoTask *task,
                                     guint input)
{
    return 2;
}

static UfoTaskMode
ufo_zmq_sub_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_GENERATOR;
}


static gboolean
ufo_zmq_sub_task_generate (UfoTask *task,
                           UfoBuffer *output,
                           UfoRequisition *requisition)
{
    UfoZmqSubTaskPrivate *priv;
    zmq_msg_t msg;
    gsize size;

    priv = UFO_ZMQ_SUB_TASK_GET_PRIVATE (task);

    if (priv->stop)
        return FALSE;

    size = ufo_buffer_get_size (output);
    zmq_msg_init_size (&msg, size);
    zmq_msg_recv (&msg, priv->socket, 0);
    g_assert (zmq_msg_size (&msg) == size);
    memcpy (ufo_buffer_get_host_array (output, NULL), zmq_msg_data (&msg), size);
    zmq_msg_close (&msg);

    return TRUE;
}

static void
ufo_zmq_sub_task_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
    UfoZmqSubTaskPrivate *priv = UFO_ZMQ_SUB_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_ADDRESS:
            g_free (priv->address);
            priv->address = g_value_dup_string (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_zmq_sub_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoZmqSubTaskPrivate *priv = UFO_ZMQ_SUB_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_ADDRESS:
            g_value_set_string (value, priv->address);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_zmq_sub_task_finalize (GObject *object)
{
    UfoZmqSubTaskPrivate *priv;

    priv = UFO_ZMQ_SUB_TASK_GET_PRIVATE (object);
    zmq_close (priv->socket);
    zmq_ctx_destroy (priv->context);
    g_free (priv->address);

    G_OBJECT_CLASS (ufo_zmq_sub_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_zmq_sub_task_setup;
    iface->get_num_inputs = ufo_zmq_sub_task_get_num_inputs;
    iface->get_num_dimensions = ufo_zmq_sub_task_get_num_dimensions;
    iface->get_mode = ufo_zmq_sub_task_get_mode;
    iface->get_requisition = ufo_zmq_sub_task_get_requisition;
    iface->generate = ufo_zmq_sub_task_generate;
}

static void
ufo_zmq_sub_task_class_init (UfoZmqSubTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_zmq_sub_task_set_property;
    oclass->get_property = ufo_zmq_sub_task_get_property;
    oclass->finalize = ufo_zmq_sub_task_finalize;

    properties[PROP_ADDRESS] =
        g_param_spec_string ("address",
            "ZMQ address to subscribe to",
            "ZMQ address to subscribe to",
            "tcp://127.0.0.1",
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoZmqSubTaskPrivate));
}

static void
ufo_zmq_sub_task_init(UfoZmqSubTask *self)
{
    self->priv = UFO_ZMQ_SUB_TASK_GET_PRIVATE(self);
    self->priv->context = NULL;
    self->priv->socket = NULL;
    self->priv->address = g_strdup ("tcp://127.0.0.1");
    self->priv->id = (gint32) g_random_int ();
    self->priv->stop = FALSE;
}
