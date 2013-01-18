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

#ifndef __UFO_TASK_NODE_H
#define __UFO_TASK_NODE_H

#include <ufo-node.h>
#include <ufo-group.h>

G_BEGIN_DECLS

#define UFO_TYPE_TASK_NODE             (ufo_task_node_get_type())
#define UFO_TASK_NODE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_TASK_NODE, UfoTaskNode))
#define UFO_IS_TASK_NODE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_TASK_NODE))
#define UFO_TASK_NODE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_TASK_NODE, UfoTaskNodeClass))
#define UFO_IS_TASK_NODE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_TASK_NODE))
#define UFO_TASK_NODE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_TASK_NODE, UfoTaskNodeClass))

typedef struct _UfoTaskNode           UfoTaskNode;
typedef struct _UfoTaskNodeClass      UfoTaskNodeClass;
typedef struct _UfoTaskNodePrivate    UfoTaskNodePrivate;

/**
 * UfoSendPattern:
 * @UFO_SEND_BROADCAST: Broadcast data to all connected nodes
 * @UFO_SEND_SCATTER: Scatter data among connected nodes.
 *
 * The send pattern describes how results are passed to connected nodes.
 */
typedef enum {
    UFO_SEND_BROADCAST,
    UFO_SEND_SCATTER
} UfoSendPattern;

/**
 * UfoTaskNode:
 *
 * Main object for organizing filters. The contents of the #UfoTaskNode structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoTaskNode {
    /*< private >*/
    UfoNode parent_instance;

    UfoTaskNodePrivate *priv;
};

/**
 * UfoTaskNodeClass:
 *
 * #UfoTaskNode class
 */
struct _UfoTaskNodeClass {
    /*< private >*/
    UfoNodeClass parent_class;
};

void            ufo_task_node_set_plugin_name       (UfoTaskNode    *task_node,
                                                     const gchar    *name);
const gchar    *ufo_task_node_get_plugin_name       (UfoTaskNode    *task_node);
const gchar    *ufo_task_node_get_unique_name       (UfoTaskNode    *task_node);
void            ufo_task_node_set_send_pattern      (UfoTaskNode    *task_node,
                                                     UfoSendPattern  pattern);
UfoSendPattern  ufo_task_node_get_send_pattern      (UfoTaskNode    *task_node);
void            ufo_task_node_set_out_group         (UfoTaskNode    *task_node,
                                                     UfoGroup       *group);
UfoGroup       *ufo_task_node_get_out_group         (UfoTaskNode    *task_node);
void            ufo_task_node_add_in_group          (UfoTaskNode    *task_node,
                                                     guint           pos,
                                                     UfoGroup       *group);
UfoGroup       *ufo_task_node_get_current_in_group  (UfoTaskNode    *task_node,
                                                     guint           pos);
void            ufo_task_node_switch_in_group       (UfoTaskNode    *task_node,
                                                     guint           pos);
void            ufo_task_node_set_proc_node         (UfoTaskNode    *task_node,
                                                     UfoNode        *proc_node);
UfoNode        *ufo_task_node_get_proc_node         (UfoTaskNode    *task_node);
GType           ufo_task_node_get_type              (void);

G_END_DECLS

#endif