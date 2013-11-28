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

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <ufo/ufo-node.h>
#include <ufo/ufo-group.h>
#include <ufo/ufo-profiler.h>

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

void            ufo_task_node_set_plugin_name       (UfoTaskNode    *node,
                                                     const gchar    *name);
const gchar    *ufo_task_node_get_plugin_name       (UfoTaskNode    *node);
const gchar    *ufo_task_node_get_unique_name       (UfoTaskNode    *node);
void            ufo_task_node_set_send_pattern      (UfoTaskNode    *node,
                                                     UfoSendPattern  pattern);
UfoSendPattern  ufo_task_node_get_send_pattern      (UfoTaskNode    *node);
void            ufo_task_node_set_num_expected      (UfoTaskNode    *node,
                                                     guint           pos,
                                                     gint            n_expected);
gint            ufo_task_node_get_num_expected      (UfoTaskNode    *node,
                                                     guint           pos);
void            ufo_task_node_set_out_group         (UfoTaskNode    *node,
                                                     UfoGroup       *group);
UfoGroup       *ufo_task_node_get_out_group         (UfoTaskNode    *node);
GList          *ufo_task_node_get_in_groups         (UfoTaskNode    *node);
void            ufo_task_node_add_in_group          (UfoTaskNode    *node,
                                                     guint           pos,
                                                     UfoGroup       *group);
UfoGroup       *ufo_task_node_get_current_in_group  (UfoTaskNode    *node,
                                                     guint           pos);
void            ufo_task_node_switch_in_group       (UfoTaskNode    *node,
                                                     guint           pos);
void            ufo_task_node_set_own_group         (UfoTaskNode *node,
                                                     UfoGroup *group);
UfoGroup       *ufo_task_node_get_own_group         (UfoTaskNode *node);
void            ufo_task_node_set_proc_node         (UfoTaskNode    *task_node,
                                                     UfoNode        *proc_node);
UfoNode        *ufo_task_node_get_proc_node         (UfoTaskNode    *node);
void            ufo_task_node_set_partition         (UfoTaskNode    *node,
                                                     guint           index,
                                                     guint           total);
GAsyncQueue    *ufo_task_node_get_input_queue       (UfoTaskNode *node);
GAsyncQueue    *ufo_task_node_get_output_queue      (UfoTaskNode *node);
void            ufo_task_node_get_partition         (UfoTaskNode    *node,
                                                     guint          *index,
                                                     guint          *total);
void            ufo_task_node_set_profiler          (UfoTaskNode    *node,
                                                     UfoProfiler    *profiler);
UfoProfiler    *ufo_task_node_get_profiler          (UfoTaskNode    *node);
void            ufo_task_node_increase_processed    (UfoTaskNode    *node);
GType           ufo_task_node_get_type              (void);

G_END_DECLS

#endif
