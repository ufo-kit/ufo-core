#ifndef __UFO_REMOTE_NODE_H
#define __UFO_REMOTE_NODE_H

#include <glib-object.h>
#include "ufo-node.h"
#include "ufo-buffer.h"
#include "ufo-task-iface.h"

G_BEGIN_DECLS

#define UFO_TYPE_REMOTE_NODE             (ufo_remote_node_get_type())
#define UFO_REMOTE_NODE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_REMOTE_NODE, UfoRemoteNode))
#define UFO_IS_REMOTE_NODE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_REMOTE_NODE))
#define UFO_REMOTE_NODE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_REMOTE_NODE, UfoRemoteNodeClass))
#define UFO_IS_REMOTE_NODE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_REMOTE_NODE))
#define UFO_REMOTE_NODE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_REMOTE_NODE, UfoRemoteNodeClass))

typedef struct _UfoRemoteNode           UfoRemoteNode;
typedef struct _UfoRemoteNodeClass      UfoRemoteNodeClass;
typedef struct _UfoRemoteNodePrivate    UfoRemoteNodePrivate;
typedef struct _UfoMessage              UfoMessage;

/**
 * UfoRemoteNode:
 *
 * Main object for organizing filters. The contents of the #UfoRemoteNode structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoRemoteNode {
    /*< private >*/
    UfoNode parent_instance;

    UfoRemoteNodePrivate *priv;
};

/**
 * UfoRemoteNodeClass:
 *
 * #UfoRemoteNode class
 */
struct _UfoRemoteNodeClass {
    /*< private >*/
    UfoNodeClass parent_class;
};

typedef enum {
    UFO_MESSAGE_SETUP = 0,
    UFO_MESSAGE_GET_STRUCTURE,
    UFO_MESSAGE_STRUCTURE,
    UFO_MESSAGE_GET_REQUISITION,
    UFO_MESSAGE_REQUISITION,
    UFO_MESSAGE_SEND_INPUTS,
    UFO_MESSAGE_GET_RESULT,
    UFO_MESSAGE_RESULT,
    UFO_MESSAGE_ACK
} UfoMessageType;

struct _UfoMessage {
    UfoMessageType  type;

    guint n_inputs;
};

UfoNode  *ufo_remote_node_new               (gpointer     zmq_context,
                                             const gchar *address);
void      ufo_remote_node_request_setup     (UfoRemoteNode *node);
void      ufo_remote_node_get_structure     (UfoRemoteNode *node,
                                             guint *n_inputs,
                                             UfoInputParameter **in_params);
void      ufo_remote_node_send_inputs       (UfoRemoteNode *node,
                                             UfoBuffer **inputs);
void      ufo_remote_node_get_result        (UfoRemoteNode *node,
                                             UfoBuffer *result);
void      ufo_remote_node_get_requisition   (UfoRemoteNode *node,
                                             UfoRequisition *requisition);
GType     ufo_remote_node_get_type          (void);

G_END_DECLS

#endif
