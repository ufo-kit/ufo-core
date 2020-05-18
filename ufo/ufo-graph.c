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

#include "config.h"

#include <stdio.h>

#include "ufo-node.h"
#include "ufo-task-node.h"
#include "ufo-graph.h"
#include "ufo-priv.h"

/**
 * SECTION:ufo-graph
 * @Short_description: Generic graph structure
 * @Title: UfoGraph
 */

G_DEFINE_TYPE (UfoGraph, ufo_graph, G_TYPE_OBJECT)

#define UFO_GRAPH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GRAPH, UfoGraphPrivate))

struct _UfoGraphPrivate {
    GList *nodes;
    GList *edges;
    GList *copies;
};

enum {
    PROP_0,
    N_PROPERTIES
};

static gint cmp_edge (gconstpointer a, gconstpointer b);
static gint cmp_edge_source (gconstpointer a, gconstpointer b);
static gint cmp_edge_target (gconstpointer a, gconstpointer b);
static UfoEdge *find_edge (GList *edges, UfoNode *source, UfoNode *target);
static GList *g_list_find_all_data (GList *list, gconstpointer data, GCompareFunc func);

/**
 * ufo_graph_new:
 *
 * Create a new #UfoGraph object.
 *
 * Returns: (transfer full): A #UfoGraph.
 */
UfoGraph *
ufo_graph_new (void)
{
    return UFO_GRAPH (g_object_new (UFO_TYPE_GRAPH, NULL));
}

/**
 * ufo_graph_is_connected:
 * @graph: A #UfoGraph
 * @from: A source node
 * @to: A target node
 *
 * Check whether @from is connected to @to.
 *
 * Returns: %TRUE if @from is connected to @to, otherwise %FALSE.
 */
gboolean
ufo_graph_is_connected (UfoGraph *graph,
                        UfoNode *from,
                        UfoNode *to)
{
    UfoGraphPrivate *priv;
    UfoEdge *edge;

    g_return_val_if_fail (UFO_IS_GRAPH (graph), FALSE);
    priv = graph->priv;
    edge = find_edge (priv->edges, from, to);
    return edge != NULL;
}

static void
add_node_if_not_found (UfoGraphPrivate *priv,
                       UfoNode *node)
{
    if (!g_list_find (priv->nodes, node)) {
        priv->nodes = g_list_append (priv->nodes, node);
        g_object_ref (node);
    }
}

/**
 * ufo_graph_connect_nodes:
 * @graph: A #UfoGraph
 * @source: A source node
 * @target: A target node
 * @label: An arbitrary label
 *
 * Connect @source with @target in @graph and annotate the edge with
 * @label.
 */
void
ufo_graph_connect_nodes (UfoGraph *graph,
                         UfoNode *source,
                         UfoNode *target,
                         gpointer label)
{
    UfoGraphPrivate *priv;
    UfoEdge *edge;

    g_return_if_fail (UFO_IS_GRAPH (graph));
    priv = graph->priv;

    if (ufo_graph_is_connected (graph, source, target) &&
        ufo_graph_get_edge_label (graph, source, target) == label) {
        return;
    }

    edge = g_new0 (UfoEdge, 1);
    edge->source = source;
    edge->target = target;
    edge->label = label;

    priv->edges = g_list_append (priv->edges, edge);

    add_node_if_not_found (priv, source);
    add_node_if_not_found (priv, target);
}

/**
 * ufo_graph_get_num_nodes:
 * @graph: A #UfoGraph
 *
 * Get number of nodes in @graph. The number is always divisible by two, because
 * nodes are only part of a graph if member of an edge.
 * Returns: Number of nodes.
 */
guint
ufo_graph_get_num_nodes (UfoGraph *graph)
{
    g_return_val_if_fail (UFO_IS_GRAPH (graph), 0);
    return g_list_length (graph->priv->nodes);
}

/**
 * ufo_graph_get_num_edges:
 * @graph: A #UfoGraph
 *
 * Get number of edges present in @graph.
 *
 * Returns: Number of edges.
 */
guint
ufo_graph_get_num_edges (UfoGraph *graph)
{
    g_return_val_if_fail (UFO_IS_GRAPH (graph), 0);
    return g_list_length (graph->priv->edges);
}

/**
 * ufo_graph_get_edges:
 * @graph: A #UfoGraph
 *
 * Get all edges contained in @graph.
 *
 * Returns: (transfer full) (element-type UfoEdge): a list of #UfoEdge elements or %NULL on
 * error. Release the list with g_list_free().
 */
GList *
ufo_graph_get_edges (UfoGraph *graph)
{
    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    return g_list_copy (graph->priv->edges);
}

/**
 * ufo_graph_get_nodes:
 * @graph: A #UfoGraph
 *
 * Returns: (element-type UfoNode) (transfer container): A list of all nodes
 * added to @graph.
 */
GList *
ufo_graph_get_nodes (UfoGraph *graph)
{
    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    return g_list_copy (graph->priv->nodes);
}

/**
 * ufo_graph_get_nodes_filtered:
 * @graph: A #UfoGraph
 * @func: (scope call): Predicate function to filter out nodes
 * @user_data: Data to be passed to @func on invocation
 *
 * Get nodes filtered by the predicate @func.
 *
 * Returns: (element-type UfoNode) (transfer container): A list of all nodes
 * that are marked as true by the predicate function @func.
 */
GList *
ufo_graph_get_nodes_filtered (UfoGraph *graph,
                              UfoFilterPredicate func,
                              gpointer user_data)
{
    UfoGraphPrivate *priv;
    GList *it;
    GList *result = NULL;

    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    priv = graph->priv;

    g_list_for (priv->nodes, it) {
        UfoNode *node = UFO_NODE (it->data);

        if (func (node, user_data))
            result = g_list_append (result, node);
    }

    return result;
}

/**
 * ufo_graph_remove_edge:
 * @graph: A #UfoGraph
 * @source: A source node
 * @target: A target node
 *
 * Remove edge between @source and @target.
 */
void
ufo_graph_remove_edge (UfoGraph *graph,
                       UfoNode *source,
                       UfoNode *target)
{
    UfoGraphPrivate *priv;
    UfoEdge *edge;

    g_return_if_fail (UFO_IS_GRAPH (graph));
    priv = graph->priv;
    edge = find_edge (priv->edges, source, target);

    if (edge != NULL) {
        priv->nodes = g_list_remove (priv->nodes, source);
        g_object_unref (source);

        priv->nodes = g_list_remove (priv->nodes, target);
        g_object_unref (target);

        priv->edges = g_list_remove (priv->edges, edge);
    }
}

/**
 * ufo_graph_get_edge_label:
 * @graph: A #UfoGraph
 * @source: Source node
 * @target: Target node
 *
 * Retrieve edge label between @source and @target.
 *
 * Returns: (transfer none): Edge label pointer.
 */
gpointer
ufo_graph_get_edge_label (UfoGraph *graph,
                          UfoNode *source,
                          UfoNode *target)
{
    UfoGraphPrivate *priv;
    UfoEdge *edge;

    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    priv = graph->priv;
    edge = find_edge (priv->edges, source, target);

    if (edge != NULL)
        return edge->label;

    g_warning ("target not found");
    return NULL;
}

static gboolean
has_no_predecessor (UfoNode *node,
                    UfoGraph *graph)
{
    GList *it;

    g_list_for (graph->priv->nodes, it) {
        UfoNode *source = (UfoNode *) it->data;

        if (ufo_graph_is_connected (graph, source, node))
            return FALSE;
    }

    return TRUE;
}

/**
 * ufo_graph_get_roots:
 * @graph: A #UfoGraph
 *
 * Get all roots of @graph.
 *
 * Returns: (element-type UfoNode) (transfer container): A list of all nodes
 * that do not have a predessor node.
 */
GList *
ufo_graph_get_roots (UfoGraph *graph)
{
    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    return ufo_graph_get_nodes_filtered (graph, (UfoFilterPredicate ) has_no_predecessor, graph);
}

static gboolean
has_no_successor (UfoNode *node,
                  UfoGraph *graph)
{
    GList *it;

    g_list_for (graph->priv->nodes, it) {
        UfoNode *target = (UfoNode *) it->data;

        if (ufo_graph_is_connected (graph, node, target))
            return FALSE;
    }

    return TRUE;
}

/**
 * ufo_graph_get_leaves:
 * @graph: A #UfoGraph
 *
 * Get all leaves of @graph.
 *
 * Returns: (element-type UfoNode) (transfer container): A list of all nodes
 * that do not have a predessor node.
 */
GList *
ufo_graph_get_leaves (UfoGraph *graph)
{
    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    return ufo_graph_get_nodes_filtered (graph, (UfoFilterPredicate) has_no_successor, graph);
}

static GList *
get_target_edges (GList *edges,
                  UfoNode *target)
{
    UfoEdge match;

    match.target = target;
    return g_list_find_all_data (edges, &match, cmp_edge_target);
}

static GList *
get_source_edges (GList *edges,
                  UfoNode *source)
{
    UfoEdge match;

    match.source = source;
    return g_list_find_all_data (edges, &match, cmp_edge_source);
}

/**
 * ufo_graph_get_predecessors:
 * @graph: A #UfoGraph
 * @node: A #UfoNode whose predecessors are returned.
 *
 * Get the all nodes connected to @node.
 *
 * Returns: (element-type UfoNode) (transfer container): A list with preceeding
 * nodes of @node. Free the list with g_list_free() but not its elements.
 */
GList *
ufo_graph_get_predecessors (UfoGraph *graph,
                            UfoNode *node)
{
    UfoGraphPrivate *priv;
    GList *edges;
    GList *it;
    GList *result;

    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    priv = graph->priv;
    edges = get_target_edges (priv->edges, node);
    result = NULL;

    g_list_for (edges, it) {
        UfoEdge *edge = (UfoEdge *) it->data;
        result = g_list_prepend (result, edge->source);
    }

    g_list_free (edges);
    return result;
}

guint
ufo_graph_get_num_predecessors (UfoGraph *graph,
                                UfoNode *node)
{
    UfoGraphPrivate *priv;
    GList *edges;
    guint n_predecessors;

    g_return_val_if_fail (UFO_IS_GRAPH (graph), 0);
    priv = graph->priv;
    edges = get_target_edges (priv->edges, node);
    n_predecessors = g_list_length (edges);
    g_list_free (edges);
    return n_predecessors;
}

/**
 * ufo_graph_get_successors:
 * @graph: A #UfoGraph
 * @node: A #UfoNode whose successors are returned.
 *
 * Get the successors of @node.
 *
 * Returns: (element-type UfoNode) (transfer container): A list with succeeding
 * nodes of @node. Free the list with g_list_free() but not its elements.
 */
GList *
ufo_graph_get_successors (UfoGraph *graph,
                          UfoNode *node)
{
    UfoGraphPrivate *priv;
    GList *edges;
    GList *it;
    GList *result;

    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    priv = graph->priv;
    edges = get_source_edges (priv->edges, node);
    result = NULL;

    g_list_for (edges, it) {
        UfoEdge *edge = (UfoEdge *) it->data;
        result = g_list_append (result, edge->target);
    }

    g_list_free (edges);
    return result;
}

guint
ufo_graph_get_num_successors (UfoGraph *graph,
                              UfoNode *node)
{
    UfoGraphPrivate *priv;
    GList *edges;
    guint n_successors;

    g_return_val_if_fail (UFO_IS_GRAPH (graph), 0);
    priv = graph->priv;
    edges = get_source_edges (priv->edges, node);
    n_successors = g_list_length (edges);
    g_list_free (edges);
    return n_successors;
}

/**
 * ufo_graph_expand:
 * @graph: A #UfoGraph
 * @path: (element-type UfoNode): A path of nodes.
 * @error: error to pass on
 *
 * Duplicate nodes between head and tail of path and insert at the exact the
 * position of where path started and ended.
 */
void
ufo_graph_expand (UfoGraph *graph,
                  GList *path,
                  GError **error)
{
    GList *head;
    GList *tail;
    UfoNode *orig;
    UfoNode *current;
    GError *tmp_error = NULL;

    g_return_if_fail (UFO_IS_GRAPH (graph));

    head = g_list_first (path);
    tail = g_list_last (path);

    if (head == tail)
        return;

    orig = UFO_NODE (head->data);

    /* The first link goes from the original head */
    current = orig;

    for (GList *it = g_list_next (head); it != tail; it = g_list_next (it)) {
        UfoNode *next;
        UfoNode *copy;
        gpointer label;

        next = UFO_NODE (it->data);

        /*
         * Do not copy node if it has more than one input because input data
         * cannot be reliably associated
         */
        if (ufo_graph_get_num_predecessors (graph, next) <= 1) {
            copy = ufo_node_copy (next, &tmp_error);
            if (tmp_error != NULL) {
                g_propagate_prefixed_error (error, tmp_error,
                                            "%s: ", ufo_task_node_get_plugin_name (UFO_TASK_NODE (next)));
                return;
            }
            label = ufo_graph_get_edge_label (graph, orig, next);
            ufo_graph_connect_nodes (graph, current, copy, label);
            graph->priv->copies = g_list_append (graph->priv->copies, copy);
            current = copy;
        }
        else {
            label = ufo_graph_get_edge_label (graph, orig, next);
            ufo_graph_connect_nodes (graph, current, next, label);
            current = next;
        }

        orig = next;
    }

    if (tail->data != NULL) {
        ufo_graph_connect_nodes (graph, current, UFO_NODE (tail->data),
                                 ufo_graph_get_edge_label (graph, orig, UFO_NODE (tail->data)));
    }
}

static UfoNode *
find_first_match (UfoGraph *graph,
                  UfoNode *start,
                  UfoFilterPredicate pred,
                  gpointer user_data)
{
    GList *successors;
    GList *it;
    UfoNode *result = NULL;

    if (start == NULL)
        return NULL;

    if (pred (start, user_data))
        return start;

    successors = ufo_graph_get_successors (graph, start);

    g_list_for (successors, it) {
        result = find_first_match (graph, it->data, pred, user_data);

        if (result != NULL)
            break;
    }

    g_list_free (successors);
    return result;
}

static GList *
follow_path (UfoGraph *graph,
             GList *current,
             UfoFilterPredicate pred,
             gpointer user_data)
{
    GList *it;
    GList *last;
    GList *successors;

    if (current == NULL)
        return NULL;

    last = g_list_last (current);
    successors = ufo_graph_get_successors (graph, last->data);

    g_list_for (successors, it) {
        if (pred (it->data, user_data)) {
            current = g_list_append (current, it->data);
            current = follow_path (graph, current, pred, user_data);
            break;
        }
    }

    g_list_free (successors);
    return current;
}

/**
 * ufo_graph_find_longest_path:
 * @graph: A #UfoGraph
 * @pred: (scope call): Predicate function for which elements of the path must
 *      evaluate to %TRUE.
 * @user_data: User data passed to @pred.
 *
 * Find the longest path in @task_graph that fulfills @pred.
 *
 * Returns: (transfer full) (element-type UfoNode): A list with nodes in
 * subsequent order of the path. User must free it with g_list_free.
 */
GList *
ufo_graph_find_longest_path (UfoGraph *graph,
                             UfoFilterPredicate pred,
                             gpointer user_data)
{
    GList *it;
    GList *roots;
    GList *candidates;
    GHashTable *starts;
    guint length = 0;
    GList *result = NULL;

    roots = ufo_graph_get_roots (graph);

    /* we use the hash table to simulate a set with unique members */
    starts = g_hash_table_new (g_direct_hash, g_direct_equal);

    g_list_for (roots, it) {
        UfoNode *start;

        start = find_first_match (graph, it->data, pred, user_data);

        if (start != NULL)
            g_hash_table_insert (starts, start, start);
    }

    candidates = g_hash_table_get_keys (starts);

    g_list_for (candidates, it) {
        GList *path;

        path = follow_path (graph, g_list_append (NULL, it->data), pred, user_data);

        if (g_list_length (path) > length) {
            length = g_list_length (path);
            result = path;
        }
        else
            g_list_free (path);
    }

    g_list_free (candidates);
    g_list_free (roots);
    g_hash_table_destroy (starts);

    return result;
}

/**
 * ufo_graph_dump_dot:
 * @graph: A #UfoGraph
 * @filename: A string containing a filename
 *
 * Stores a GraphViz dot representation of @graph in @filename.
 */
void
ufo_graph_dump_dot (UfoGraph *graph,
                    const gchar *filename)
{
    FILE *fp;
    GList *nodes;
    GList *it;

    fp = fopen (filename, "w");
    fprintf (fp, "digraph foo {\n");

    nodes = ufo_graph_get_nodes (graph);

    g_list_for (nodes, it) {
        UfoNode *source;
        GList *successors;
        GList *jt;

        source = UFO_NODE (it->data);
        successors = ufo_graph_get_successors (graph, source);

        g_list_for (successors, jt) {
            UfoNode *target;

            target = UFO_NODE (jt->data);

            fprintf (fp, "  %s_%p -> %s_%p;\n",
                     g_type_name (G_TYPE_FROM_INSTANCE (source)), (gpointer) source,
                     g_type_name (G_TYPE_FROM_INSTANCE (target)), (gpointer) target);
        }

        g_list_free (successors);
    }

    g_list_free (nodes);
    fprintf (fp, "}\n");
    fclose (fp);
}

static gint
cmp_edge (gconstpointer a, gconstpointer b)
{
    const UfoEdge *edge_a = a;
    const UfoEdge *edge_b = b;

    if ((edge_a->source == edge_b->source) &&
        (edge_a->target == edge_b->target)) {
        return 0;
    }

    return -1;
}

static gint
cmp_edge_source (gconstpointer a, gconstpointer b)
{
    const UfoEdge *edge_a = a;
    const UfoEdge *edge_b = b;

    if (edge_a->source == edge_b->source)
        return 0;

    return -1;
}

static gint
cmp_edge_target (gconstpointer a, gconstpointer b)
{
    const UfoEdge *edge_a = a;
    const UfoEdge *edge_b = b;

    if (edge_a->target == edge_b->target)
        return 0;

    return -1;
}

static GList *
g_list_find_all_data (GList *list,
                      gconstpointer data,
                      GCompareFunc func)
{
    GList *it;
    GList *result = NULL;

    g_list_for (list, it) {
        if (func (data, it->data) == 0)
            result = g_list_prepend (result, it->data);
    }

    return result;
}

static UfoEdge *
find_edge (GList *edges,
           UfoNode *source,
           UfoNode *target)
{
    UfoEdge search_edge;
    UfoEdge *edge;
    GList *result;

    search_edge.source = source;
    search_edge.target = target;
    result = g_list_find_custom (edges, &search_edge, cmp_edge);
    edge = result != NULL ? g_list_nth_data (result, 0) : NULL;
    return edge;
}

static void
ufo_graph_dispose (GObject *object)
{
    UfoGraphPrivate *priv;

    priv = UFO_GRAPH_GET_PRIVATE (object);

    if (priv->edges != NULL) {
        g_list_foreach (priv->edges, (GFunc) g_free, NULL);
        g_list_free (priv->edges);
        priv->edges = NULL;
    }

    if (priv->nodes != NULL) {
        g_list_foreach (priv->nodes, (GFunc) g_object_unref, NULL);
        g_list_free (priv->nodes);
        priv->nodes = NULL;
    }

    if (priv->copies != NULL) {
        g_list_foreach (priv->copies, (GFunc) g_object_unref, NULL);
        g_list_free (priv->copies);
        priv->copies = NULL;
    }

    G_OBJECT_CLASS (ufo_graph_parent_class)->dispose (object);
}

static void
ufo_graph_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_graph_parent_class)->finalize (object);
}

static void
ufo_graph_class_init (UfoGraphClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->dispose  = ufo_graph_dispose;
    oclass->finalize = ufo_graph_finalize;

    g_type_class_add_private(klass, sizeof(UfoGraphPrivate));
}

static void
ufo_graph_init (UfoGraph *self)
{
    UfoGraphPrivate *priv;
    self->priv = priv = UFO_GRAPH_GET_PRIVATE (self);
    priv->nodes = NULL;
    priv->copies = NULL;
}
