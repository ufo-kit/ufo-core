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
#include "ufo-zmq-pub-task.h"
#include "ufo-zmq-common.h"


struct _UfoZmqPubTaskPrivate {
    gpointer context;
    gpointer socket;
    guint expected_subscribers;
    guint64 current;
    GHashTable *counts;
    JsonBuilder *builder;
    JsonGenerator *generator;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoZmqPubTask, ufo_zmq_pub_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_ZMQ_PUB_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_ZMQ_PUB_TASK, UfoZmqPubTaskPrivate))

enum {
    PROP_0,
    PROP_EXPECTED_SUBSCRIBERS,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_zmq_pub_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_ZMQ_PUB_TASK, NULL));
}

static gboolean
handle_registration (UfoZmqPubTaskPrivate *priv, ZmqRequest *request, gboolean insert)
{
    zmq_msg_t msg;
    ZmqReply *reply;
    gboolean success = TRUE;

    zmq_msg_init_size (&msg, sizeof (ZmqReply));
    reply = zmq_msg_data (&msg);
    reply->error = ZMQ_ERROR_OKAY;
    reply->type = ZMQ_REPLY_ACK;

    if (request->type != ZMQ_REQUEST_REGISTER) {
        reply->error = ZMQ_ERROR_REGISTRATION_EXPECTED;
        success = FALSE;
        goto send_registration_reply;
    }

    if (g_hash_table_lookup (priv->counts, GINT_TO_POINTER (request->id)) != NULL) {
        reply->error = ZMQ_ERROR_ALREADY_REGISTERED;
        success = FALSE;
        goto send_registration_reply;
    }

    if (insert)
        g_hash_table_insert (priv->counts, GINT_TO_POINTER (request->id), GINT_TO_POINTER (1));

send_registration_reply:
    g_assert (zmq_msg_send (&msg, priv->socket, 0) >= 0);
    return success;
}

static void
ufo_zmq_pub_task_setup (UfoTask *task,
                        UfoResources *resources,
                        GError **error)
{
    UfoZmqPubTaskPrivate *priv;

    priv = UFO_ZMQ_PUB_TASK_GET_PRIVATE (task);

    priv->context = zmq_ctx_new ();
    priv->current = 0;

    if (priv->context == NULL) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "zmq context creation failed: %s\n", zmq_strerror (zmq_errno ()));
        return;
    }

    priv->socket = zmq_socket (priv->context, ZMQ_REP);

    if (priv->socket == NULL) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "zmq pub_socket creation failed: %s\n", zmq_strerror (zmq_errno ()));
        return;
    }

    if (zmq_bind (priv->socket, "tcp://*:5555") != 0) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "zmq bind failed: %s\n", zmq_strerror (zmq_errno ()));
        return;
    }

    for (guint registered = 0; registered < priv->expected_subscribers; ) {
        zmq_msg_t msg;
        ZmqRequest *request;

        zmq_msg_init_size (&msg, sizeof (ZmqRequest));
        zmq_msg_recv (&msg, priv->socket, 0);

        request = zmq_msg_data (&msg);

        if (handle_registration (priv, request, TRUE))
            registered++;

        zmq_msg_close (&msg);
    }
}

static void
ufo_zmq_pub_task_get_requisition (UfoTask *task,
                                  UfoBuffer **inputs,
                                  UfoRequisition *requisition,
                                  GError **error)
{
    requisition->n_dims = 0;
}

static guint
ufo_zmq_pub_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_zmq_pub_task_get_num_dimensions (UfoTask *task,
                                     guint input)
{
    return 2;
}

static UfoTaskMode
ufo_zmq_pub_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_SINK | UFO_TASK_MODE_CPU;
}

static JsonNode *
requisition_to_json_array (UfoRequisition *requisition)
{
    JsonNode *node;
    JsonArray *array;

    array = json_array_sized_new (requisition->n_dims);

    for (guint i = 0; i < requisition->n_dims; i++) {
        JsonNode *element;
        gint64 length;

        /* start from the last dimension as per htype spec */
        length = (gint64) requisition->dims[requisition->n_dims - 1 - i];
        element = json_node_alloc ();
        json_node_init_int (element, length);
        json_array_add_element (array, element);
    }

    node = json_node_alloc ();
    json_node_init_array (node, array);

    return node;
}

static gboolean
ufo_zmq_pub_task_process (UfoTask *task,
                          UfoBuffer **inputs,
                          UfoBuffer *output,
                          UfoRequisition *requisition)
{
    UfoZmqPubTaskPrivate *priv;
    UfoRequisition req;
    guint num_to_serve;
    gsize size;
    gchar *src;
    JsonNode *array;
    JsonNode *tree;
    gchar *header;
    gsize header_size;
    GList *new_subscribers = NULL;

    priv = UFO_ZMQ_PUB_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], &req);
    size = ufo_buffer_get_size (inputs[0]);

    json_builder_reset (priv->builder);
    json_builder_begin_object (priv->builder);

    json_builder_set_member_name (priv->builder, "htype");
    json_builder_add_string_value (priv->builder, "array-1.0");

    json_builder_set_member_name (priv->builder, "frame");
    json_builder_add_int_value (priv->builder, priv->current);

    json_builder_set_member_name (priv->builder, "type");
    json_builder_add_string_value (priv->builder, "float");

    array = requisition_to_json_array (&req);
    json_builder_set_member_name (priv->builder, "shape");
    json_builder_add_value (priv->builder, array);

    json_builder_end_object (priv->builder);
    tree = json_builder_get_root (priv->builder);

    json_generator_set_root (priv->generator, tree);
    header = json_generator_to_data (priv->generator, &header_size);

    json_node_unref (tree);

    num_to_serve = g_hash_table_size (priv->counts);
    src = (gchar *) ufo_buffer_get_host_array (inputs[0], NULL);

    priv->current++;

    while (num_to_serve > 0) {
        zmq_msg_t request_msg;
        ZmqRequest *request;

        zmq_msg_init_size (&request_msg, sizeof (ZmqRequest));
        zmq_msg_recv (&request_msg, priv->socket, 0);

        request = zmq_msg_data (&request_msg);

        if (request->type == ZMQ_REQUEST_REGISTER) {
            if (handle_registration (priv, request, FALSE))
                new_subscribers = g_list_append (new_subscribers, GINT_TO_POINTER (request->id));
        }

        if (request->type == ZMQ_REQUEST_DATA) {
            guint count;
            zmq_msg_t reply_msg;
            ZmqReply *reply;

            zmq_msg_init_size (&reply_msg, sizeof (ZmqReply));
            reply = zmq_msg_data (&reply_msg);
            reply->type = ZMQ_REPLY_ACK;

            count = GPOINTER_TO_INT (g_hash_table_lookup (priv->counts, GINT_TO_POINTER (request->id)));

            if (count == 0) {
                reply->error = ZMQ_ERROR_NOT_REGISTERED;
                g_assert (zmq_msg_send (&reply_msg, priv->socket, 0) >= 0);
                zmq_msg_close (&reply_msg);
            }
            else if (count == priv->current + 1) {
                reply->error = ZMQ_ERROR_DATA_ALREADY_SENT;
                g_assert (zmq_msg_send (&reply_msg, priv->socket, 0) >= 0);
                zmq_msg_close (&reply_msg);
            }
            else {
                zmq_msg_t htype_msg;
                zmq_msg_t data_msg;
                gchar *dst;

                /* send ack */
                reply->error = ZMQ_ERROR_OKAY;
                g_assert (zmq_msg_send (&reply_msg, priv->socket, ZMQ_SNDMORE) >= 0);
                zmq_msg_close (&reply_msg);

                /* send geometry */
                zmq_msg_init_size (&htype_msg, header_size);
                dst = zmq_msg_data (&htype_msg);
                memcpy (dst, header, header_size);
                g_assert (zmq_msg_send (&htype_msg, priv->socket, ZMQ_SNDMORE) >= 0);
                zmq_msg_close (&htype_msg);

                /* send actual payload */
                zmq_msg_init_size (&data_msg, size);
                dst = zmq_msg_data (&data_msg);
                memcpy (dst, src, size);
                g_assert (zmq_msg_send (&data_msg, priv->socket, 0) >= 0);
                zmq_msg_close (&data_msg);

                g_hash_table_insert (priv->counts, GINT_TO_POINTER (request->id), GINT_TO_POINTER (priv->current + 1));
            }
        }

        num_to_serve--;
        zmq_msg_close (&request_msg);
    }

    /* now add new subscribers */
    for (GList *it = g_list_first (new_subscribers); it != NULL; it = g_list_next (it))
        g_hash_table_insert (priv->counts, GINT_TO_POINTER (it->data), GINT_TO_POINTER (priv->current));

    g_list_free (new_subscribers);
    g_free (header);

    return TRUE;
}

static void
ufo_zmq_pub_task_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
    UfoZmqPubTaskPrivate *priv = UFO_ZMQ_PUB_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_EXPECTED_SUBSCRIBERS:
            priv->expected_subscribers = g_value_get_uint (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_zmq_pub_task_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
    UfoZmqPubTaskPrivate *priv = UFO_ZMQ_PUB_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_EXPECTED_SUBSCRIBERS:
            g_value_set_uint (value, priv->expected_subscribers);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}
static void
ufo_zmq_pub_task_dispose (GObject *object)
{
    UfoZmqPubTaskPrivate *priv;

    priv = UFO_ZMQ_PUB_TASK_GET_PRIVATE (object);

    g_object_unref (priv->builder);
    g_object_unref (priv->generator);

    G_OBJECT_CLASS (ufo_zmq_pub_task_parent_class)->dispose (object);
}

static void
ufo_zmq_pub_task_finalize (GObject *object)
{
    UfoZmqPubTaskPrivate *priv;
    guint num_to_serve;

    priv = UFO_ZMQ_PUB_TASK_GET_PRIVATE (object);
    num_to_serve = g_hash_table_size (priv->counts);

    while (num_to_serve > 0) {
        zmq_msg_t msg;
        ZmqRequest *request;

        zmq_msg_init_size (&msg, sizeof (ZmqRequest));
        zmq_msg_recv (&msg, priv->socket, 0);

        request = zmq_msg_data (&msg);

        if (request->type == ZMQ_REQUEST_REGISTER)
            g_debug ("zmq-pub: ignoring registration request because of shutdown");

        if (request->type == ZMQ_REQUEST_DATA) {
            zmq_msg_t reply_msg;
            ZmqReply *reply;

            zmq_msg_init_size (&reply_msg, sizeof (ZmqReply));
            reply = zmq_msg_data (&reply_msg);
            reply->type = ZMQ_REPLY_STOP;
            reply->error = ZMQ_ERROR_OKAY;
            g_assert (zmq_msg_send (&reply_msg, priv->socket, 0) >= 0);
            zmq_msg_close (&reply_msg);
            num_to_serve--;
        }

        zmq_msg_close (&msg);
    }

    zmq_close (priv->socket);
    zmq_ctx_destroy (priv->context);

    g_hash_table_destroy (priv->counts);

    G_OBJECT_CLASS (ufo_zmq_pub_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_zmq_pub_task_setup;
    iface->get_num_inputs = ufo_zmq_pub_task_get_num_inputs;
    iface->get_num_dimensions = ufo_zmq_pub_task_get_num_dimensions;
    iface->get_mode = ufo_zmq_pub_task_get_mode;
    iface->get_requisition = ufo_zmq_pub_task_get_requisition;
    iface->process = ufo_zmq_pub_task_process;
}

static void
ufo_zmq_pub_task_class_init (UfoZmqPubTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_zmq_pub_task_set_property;
    oclass->get_property = ufo_zmq_pub_task_get_property;
    oclass->dispose = ufo_zmq_pub_task_dispose;
    oclass->finalize = ufo_zmq_pub_task_finalize;

    properties[PROP_EXPECTED_SUBSCRIBERS] =
        g_param_spec_uint ("expected-subscribers",
            "Number of expected subscribers",
            "Number of expected subscribers",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoZmqPubTaskPrivate));
}

static void
ufo_zmq_pub_task_init(UfoZmqPubTask *self)
{
    self->priv = UFO_ZMQ_PUB_TASK_GET_PRIVATE(self);
    self->priv->context = NULL;
    self->priv->socket = NULL;
    self->priv->expected_subscribers = 0;
    self->priv->counts = g_hash_table_new (g_direct_hash, g_direct_equal);
    self->priv->builder = json_builder_new_immutable ();
    self->priv->generator = json_generator_new ();
}
