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

#ifndef __UFO_TASK_GRAPH_H
#define __UFO_TASK_GRAPH_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <ufo/ufo-graph.h>
#include <ufo/ufo-arch-graph.h>
#include <ufo/ufo-task-node.h>
#include <ufo/ufo-plugin-manager.h>

G_BEGIN_DECLS

#define UFO_TYPE_TASK_GRAPH             (ufo_task_graph_get_type())
#define UFO_TASK_GRAPH(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_TASK_GRAPH, UfoTaskGraph))
#define UFO_IS_TASK_GRAPH(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_TASK_GRAPH))
#define UFO_TASK_GRAPH_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_TASK_GRAPH, UfoTaskGraphClass))
#define UFO_IS_TASK_GRAPH_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_TASK_GRAPH))
#define UFO_TASK_GRAPH_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_TASK_GRAPH, UfoTaskGraphClass))

#define UFO_TASK_GRAPH_ERROR            ufo_task_graph_error_quark()

typedef struct _UfoTaskGraph           UfoTaskGraph;
typedef struct _UfoTaskGraphClass      UfoTaskGraphClass;
typedef struct _UfoTaskGraphPrivate    UfoTaskGraphPrivate;


typedef enum {
    UFO_TASK_GRAPH_ERROR_JSON_KEY
} UfoTaskGraphError;

/**
 * UfoTaskGraph:
 *
 * Main object for organizing filters. The contents of the #UfoTaskGraph structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoTaskGraph {
    /*< private >*/
    UfoGraph parent_instance;

    UfoTaskGraphPrivate *priv;
};

/**
 * UfoTaskGraphClass:
 *
 * #UfoTaskGraph class
 */
struct _UfoTaskGraphClass {
    /*< private >*/
    UfoGraphClass parent_class;
};

UfoGraph    *ufo_task_graph_new                 (void);
void         ufo_task_graph_read_from_file      (UfoTaskGraph       *graph,
                                                 UfoPluginManager   *manager,
                                                 const gchar        *filename,
                                                 GError            **error);
void         ufo_task_graph_read_from_data      (UfoTaskGraph       *graph,
                                                 UfoPluginManager   *manager,
                                                 const gchar        *json,
                                                 GError            **error);
void         ufo_task_graph_save_to_json        (UfoTaskGraph       *graph,
                                                 const gchar        *filename,
                                                 GError            **error);
gchar       *ufo_task_graph_get_json_data       (UfoTaskGraph       *graph,
                                                 GError            **error);
void         ufo_task_graph_map                 (UfoTaskGraph       *task_graph,
                                                 UfoArchGraph       *arch_graph);
void         ufo_task_graph_expand              (UfoTaskGraph       *task_graph,
                                                 UfoArchGraph       *arch_graph,
                                                 gboolean            expand_remote,
                                                 gboolean            expand_gpu,
                                                 gboolean            network_writer);
void         ufo_task_graph_connect_nodes       (UfoTaskGraph       *graph,
                                                 UfoTaskNode        *n1,
                                                 UfoTaskNode        *n2);
void         ufo_task_graph_connect_nodes_full  (UfoTaskGraph       *graph,
                                                 UfoTaskNode        *n1,
                                                 UfoTaskNode        *n2,
                                                 guint               input);
void         ufo_task_graph_fuse                (UfoTaskGraph       *task_graph);
void         ufo_task_graph_set_partition       (UfoTaskGraph       *task_graph,
                                                 guint               index,
                                                 guint               total);
void         ufo_task_graph_get_partition       (UfoTaskGraph       *task_graph,
                                                 guint              *index,
                                                 guint              *total);
UfoNode *    ufo_task_graph_get_writer_node     (UfoTaskGraph       *task_graph);
GType        ufo_task_graph_get_type            (void);
GQuark       ufo_task_graph_error_quark         (void);

G_END_DECLS

#endif
