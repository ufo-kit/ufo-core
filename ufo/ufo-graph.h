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

#ifndef __UFO_GRAPH_H
#define __UFO_GRAPH_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <ufo/ufo-node.h>

G_BEGIN_DECLS

#define UFO_TYPE_GRAPH             (ufo_graph_get_type())
#define UFO_GRAPH(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_GRAPH, UfoGraph))
#define UFO_IS_GRAPH(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_GRAPH))
#define UFO_GRAPH_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_GRAPH, UfoGraphClass))
#define UFO_IS_GRAPH_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_GRAPH))
#define UFO_GRAPH_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_GRAPH, UfoGraphClass))

typedef struct _UfoGraph           UfoGraph;
typedef struct _UfoGraphClass      UfoGraphClass;
typedef struct _UfoGraphPrivate    UfoGraphPrivate;
typedef struct _UfoEdge            UfoEdge;

typedef gboolean (*UfoFilterPredicate) (UfoNode *node, gpointer user_data);

/**
 * UfoEdge:
 * @source: source node
 * @target: target node
 * @label: label
 *
 * An edge in a #UfoGraph.
 */
struct _UfoEdge {
    UfoNode     *source;
    UfoNode     *target;
    gpointer     label;
};

/**
 * UfoGraph:
 *
 * Main object for organizing filters. The contents of the #UfoGraph structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoGraph {
    /*< private >*/
    GObject parent_instance;

    UfoGraphPrivate *priv;
};

/**
 * UfoGraphClass:
 *
 * #UfoGraph class
 */
struct _UfoGraphClass {
    /*< private >*/
    GObjectClass parent_class;
};

UfoGraph   *ufo_graph_new                   (void);
void        ufo_graph_connect_nodes         (UfoGraph       *graph,
                                             UfoNode        *source,
                                             UfoNode        *target,
                                             gpointer        label);
gboolean    ufo_graph_is_connected          (UfoGraph       *graph,
                                             UfoNode        *from,
                                             UfoNode        *to);
void        ufo_graph_remove_edge           (UfoGraph       *graph,
                                             UfoNode        *source,
                                             UfoNode        *target);
gpointer    ufo_graph_get_edge_label        (UfoGraph       *graph,
                                             UfoNode        *source,
                                             UfoNode        *target);
guint       ufo_graph_get_num_nodes         (UfoGraph       *graph);
GList      *ufo_graph_get_nodes             (UfoGraph       *graph);
GList      *ufo_graph_get_nodes_filtered    (UfoGraph       *graph,
                                             UfoFilterPredicate func,
                                             gpointer        user_data);
guint       ufo_graph_get_num_edges         (UfoGraph       *graph);
GList      *ufo_graph_get_edges             (UfoGraph       *graph);
GList      *ufo_graph_get_roots             (UfoGraph       *graph);
GList      *ufo_graph_get_leaves            (UfoGraph       *graph);
guint       ufo_graph_get_num_predecessors  (UfoGraph       *graph,
                                             UfoNode        *node);
GList      *ufo_graph_get_predecessors      (UfoGraph       *graph,
                                             UfoNode        *node);
guint       ufo_graph_get_num_successors    (UfoGraph       *graph,
                                             UfoNode        *node);
GList      *ufo_graph_get_successors        (UfoGraph       *graph,
                                             UfoNode        *node);
GList      *ufo_graph_get_paths             (UfoGraph       *graph,
                                             UfoFilterPredicate pred);
GList      *ufo_graph_flatten               (UfoGraph       *graph);
void        ufo_graph_expand                (UfoGraph       *graph,
                                             GList          *path);
UfoGraph   *ufo_graph_copy                  (UfoGraph       *graph,
                                             GError        **error);
void        ufo_graph_remove_node           (UfoGraph *graph,
                                             UfoNode *node);
void        ufo_graph_dump_dot              (UfoGraph       *graph,
                                             const gchar    *filename);
GType       ufo_graph_get_type              (void);

G_END_DECLS

#endif
