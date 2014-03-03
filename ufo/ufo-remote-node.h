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

#ifndef __UFO_REMOTE_NODE_H
#define __UFO_REMOTE_NODE_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <ufo/ufo-node.h>
#include <ufo/ufo-buffer.h>
#include <ufo/ufo-task-iface.h>
#include <ufo/ufo-messenger-iface.h>

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

/**
 * UfoRemoteMode:
 * @UFO_REMOTE_MODE_STREAM: Expand task graph and execute only sub-branches
 *  remotely.
 * @UFO_REMOTE_MODE_REPLICATE: Replicate the entire task graph and execute it
 *  remotely.
 */
typedef enum {
    UFO_REMOTE_MODE_STREAM,
    UFO_REMOTE_MODE_REPLICATE
} UfoRemoteMode;



UfoNode  *ufo_remote_node_new               (const gchar    *address);
guint     ufo_remote_node_get_num_gpus      (UfoRemoteNode  *node);
guint     ufo_remote_node_get_num_cpus      (UfoRemoteNode  *node);
void      ufo_remote_node_request_setup     (UfoRemoteNode  *node);
void      ufo_remote_node_send_json         (UfoRemoteNode  *node,
                                             UfoRemoteMode   mode,
                                             const gchar    *json);
void      ufo_remote_node_get_structure     (UfoRemoteNode  *node,
                                             guint          *n_inputs,
                                             UfoInputParam **in_params,
                                             UfoTaskMode    *mode);
void      ufo_remote_node_send_inputs       (UfoRemoteNode  *node,
                                             UfoBuffer     **inputs);
void      ufo_remote_node_get_result        (UfoRemoteNode  *node,
                                             UfoBuffer      *result);
void      ufo_remote_node_get_requisition   (UfoRemoteNode  *node,
                                             UfoRequisition *requisition);
void      ufo_remote_node_cleanup           (UfoRemoteNode  *node);
void      ufo_remote_node_terminate         (UfoRemoteNode  *node);
GType     ufo_remote_node_get_type          (void);

G_END_DECLS

#endif
