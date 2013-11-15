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

#include <ufo/ufo.h>
#include "test-suite.h"

typedef struct {
    UfoGraph *graph;
    UfoGraph *sequence;
    UfoGraph *diamond;
    UfoNode *root;
    UfoNode *target1;
    UfoNode *target2;
    UfoNode *target3;
} Fixture;

typedef struct {
    const gchar *path;
    void (*test_func) (Fixture *, gconstpointer);
} TestCase;

static gpointer FOO_LABEL = GINT_TO_POINTER (0xDEADF00D);
static gpointer BAR_LABEL = GINT_TO_POINTER (0xF00BA);
static gpointer BAZ_LABEL = GINT_TO_POINTER (0xBA22BA22);

static void
fixture_setup (Fixture *fixture, gconstpointer data)
{
    fixture->graph = ufo_graph_new ();
    g_assert (UFO_IS_GRAPH (fixture->graph));

    fixture->sequence = ufo_graph_new ();
    g_assert (UFO_IS_GRAPH (fixture->sequence));

    fixture->diamond = ufo_graph_new ();
    g_assert (UFO_IS_GRAPH (fixture->diamond));

    fixture->root = ufo_node_new (FOO_LABEL);
    fixture->target1 = ufo_node_new (BAR_LABEL);
    fixture->target2 = ufo_node_new (BAZ_LABEL);
    fixture->target3 = ufo_node_new (FOO_LABEL);

    ufo_graph_connect_nodes (fixture->graph,
                             fixture->root,
                             fixture->target1,
                             FOO_LABEL);

    ufo_graph_connect_nodes (fixture->graph,
                             fixture->root,
                             fixture->target2,
                             BAR_LABEL);

    ufo_graph_connect_nodes (fixture->sequence,
                             fixture->root,
                             fixture->target1,
                             BAR_LABEL);

    ufo_graph_connect_nodes (fixture->sequence,
                             fixture->target1,
                             fixture->target2,
                             FOO_LABEL);

    ufo_graph_connect_nodes (fixture->diamond,
                             fixture->root,
                             fixture->target1,
                             BAR_LABEL);

    ufo_graph_connect_nodes (fixture->diamond,
                             fixture->root,
                             fixture->target2,
                             BAR_LABEL);

    ufo_graph_connect_nodes (fixture->diamond,
                             fixture->target1,
                             fixture->target3,
                             BAR_LABEL);

    ufo_graph_connect_nodes (fixture->diamond,
                             fixture->target2,
                             fixture->target3,
                             BAR_LABEL);
}

static void
fixture_teardown (Fixture *fixture, gconstpointer data)
{
    g_object_unref (fixture->graph);
    g_object_unref (fixture->sequence);
    g_object_unref (fixture->diamond);
    g_object_unref (fixture->target1);
    g_object_unref (fixture->target2);
    g_object_unref (fixture->target3);
}

static void
test_connected (Fixture *fixture, gconstpointer data)
{
    g_assert (ufo_graph_is_connected (fixture->sequence,
                                      fixture->root,
                                      fixture->target1));
    g_assert (ufo_graph_is_connected (fixture->sequence,
                                      fixture->target1,
                                      fixture->target2));
    g_assert (!ufo_graph_is_connected (fixture->sequence,
                                       fixture->root,
                                       fixture->target2));
    g_assert (!ufo_graph_is_connected (fixture->sequence,
                                       fixture->target1,
                                       fixture->root));
    g_assert (!ufo_graph_is_connected (fixture->sequence,
                                       fixture->target2,
                                       fixture->root));
    g_assert (!ufo_graph_is_connected (fixture->sequence,
                                       fixture->target2,
                                       fixture->target1));
}

static void
test_get_roots (Fixture *fixture, gconstpointer data)
{
    GList *roots;

    roots = ufo_graph_get_roots (fixture->graph);
    g_assert (g_list_length (roots) == 1);
    g_assert (g_list_nth_data (roots, 0) == fixture->root);
    g_list_free (roots);
}

static void
test_get_num_nodes (Fixture *fixture, gconstpointer data)
{
    g_assert (ufo_graph_get_num_nodes (fixture->graph) == 3);
    g_assert (ufo_graph_get_num_nodes (fixture->sequence) == 3);
}

static void
test_get_num_edges (Fixture *fixture, gconstpointer data)
{
    g_assert (ufo_graph_get_num_edges (fixture->graph) == 2);
    g_assert (ufo_graph_get_num_edges (fixture->sequence) == 2);
}

static void
test_get_num_successors (Fixture *fixture, gconstpointer data)
{
    g_assert (ufo_graph_get_num_successors (fixture->sequence, fixture->root) == 1);
    g_assert (ufo_graph_get_num_successors (fixture->diamond, fixture->root) == 2);
}

static void
test_get_num_predecessors (Fixture *fixture, gconstpointer data)
{
    g_assert (ufo_graph_get_num_predecessors (fixture->sequence, fixture->target1) == 1);
    g_assert (ufo_graph_get_num_predecessors (fixture->diamond, fixture->target3) == 2);
}

static void
test_get_edges (Fixture *fixture, gconstpointer data)
{
    GList *edges;
    UfoEdge *edge;

    edges = ufo_graph_get_edges (fixture->graph);
    g_assert (g_list_length (edges) == 2);

    edge = g_list_nth_data (edges, 0);
    g_assert (edge->source == fixture->root);
    g_assert (edge->target == fixture->target1 ||
              edge->target == fixture->target2);

    edge = g_list_nth_data (edges, 1);
    g_assert (edge->source == fixture->root);
    g_assert (edge->target == fixture->target1 ||
              edge->target == fixture->target2);

    g_list_free (edges);
}

static void
test_get_successors (Fixture *fixture, gconstpointer data)
{
    GList *successors;

    successors = ufo_graph_get_successors (fixture->sequence, fixture->target1);
    g_assert (g_list_length (successors) == 1);
    g_assert (g_list_nth_data (successors, 0) == fixture->target2);
    g_list_free (successors);
}

static void
test_get_predecessors (Fixture *fixture, gconstpointer data)
{
    GList *predecessors;

    predecessors = ufo_graph_get_predecessors (fixture->sequence, fixture->target2);
    g_assert (g_list_length (predecessors) == 1);
    g_assert (g_list_nth_data (predecessors, 0) == fixture->target1);
    g_list_free (predecessors);
}

static void
test_remove_edge (Fixture *fixture, gconstpointer data)
{
    GList *successors;

    ufo_graph_remove_edge (fixture->sequence, fixture->target1, fixture->target2);
    successors = ufo_graph_get_successors (fixture->sequence, fixture->target1);
    g_assert (successors == NULL);
    g_assert (g_list_length (successors) == 0);
    g_list_free (successors);
    g_assert (ufo_graph_get_num_edges (fixture->sequence) == 1);
}

static void
test_get_labels (Fixture *fixture, gconstpointer data)
{
    g_assert (ufo_graph_get_edge_label (fixture->graph, fixture->root, fixture->target1) == FOO_LABEL);
    g_assert (ufo_graph_get_edge_label (fixture->graph, fixture->root, fixture->target2) == BAR_LABEL);
}

static void
test_expansion (Fixture *fixture, gconstpointer data)
{
    GList *successors;
    UfoNode *node;
    GList *path = NULL;
    guint index;
    guint other_index;

    path = g_list_append (path, fixture->root);
    path = g_list_append (path, fixture->target1);
    path = g_list_append (path, fixture->target2);

    ufo_graph_expand (fixture->sequence, path);
    g_list_free (path);

    successors = ufo_graph_get_successors (fixture->sequence, fixture->root);
    g_assert (g_list_length (successors) == 2);

    node = UFO_NODE (g_list_nth_data (successors, 0));
    index = ufo_node_get_index (node);

    g_assert ((index == 0) || (index == 1));
    g_assert (ufo_node_get_total (node) == 2);

    node = UFO_NODE (g_list_nth_data (successors, 1));
    other_index = 1 - index;
    g_assert (ufo_node_get_index (node) == other_index);
    g_assert (ufo_node_get_total (node) == 2);

    g_list_free (successors);

    successors = ufo_graph_get_successors (fixture->sequence, node);
    g_assert (g_list_length (successors) == 1);
    node = UFO_NODE (g_list_nth_data (successors, 0));
    g_list_free (successors);

    g_assert (ufo_node_equal (node, fixture->target2));
}

static void
test_copy (Fixture *fixture, gconstpointer data)
{
    UfoGraph *copy;
    GList *roots;
    GList *successors;
    GError *error = NULL;

    copy = ufo_graph_copy (fixture->graph, &error);
    g_assert (copy != NULL);
    g_assert_no_error (error);
    g_assert (ufo_graph_get_num_edges (copy) == 2);
    g_assert (ufo_graph_get_num_nodes (copy) == 3);

    /* Check that copying preserved the order */
    roots = ufo_graph_get_roots (copy);
    g_assert (ufo_node_get_label (g_list_nth_data (roots, 0)) == FOO_LABEL);

    successors = ufo_graph_get_successors (copy,
                                           g_list_nth_data (roots, 0));

    g_assert (ufo_node_get_label (g_list_nth_data (successors, 0)) == BAR_LABEL);
    g_assert (ufo_node_get_label (g_list_nth_data (successors, 1)) == BAZ_LABEL);
    g_list_free (successors);
    g_list_free (roots);
    g_object_unref (copy);

    copy = ufo_graph_copy (fixture->sequence, &error);
    g_assert (copy != NULL);
    g_assert_no_error (error);
    g_assert (ufo_graph_get_num_edges (copy) == 2);
    g_assert (ufo_graph_get_num_nodes (copy) == 3);
    g_object_unref (copy);

    copy = ufo_graph_copy (fixture->diamond, &error);
    g_assert (copy != NULL);
    g_assert_no_error (error);
    g_assert (ufo_graph_get_num_edges (copy) == 4);
    g_assert (ufo_graph_get_num_nodes (copy) == 4);
    g_object_unref (copy);
}

static gboolean
always_true (UfoNode *node, gpointer user_data)
{
    return TRUE;
}

static void
test_get_nodes_filtered (Fixture *fixture, gconstpointer data)
{
    GList *nodes;

    nodes = ufo_graph_get_nodes_filtered (fixture->sequence, always_true, NULL);
    g_assert (g_list_length (nodes) == 3);
    g_assert (g_list_find (nodes, fixture->root) != NULL);
    g_assert (g_list_find (nodes, fixture->target1) != NULL);
    g_assert (g_list_find (nodes, fixture->target2) != NULL);
    g_list_free (nodes);
}

static void
test_flatten (Fixture *fixture, gconstpointer data)
{
    GList *levels;
    GList *roots;
    GList *second_level;
    GList *third_level;

    levels = ufo_graph_flatten (fixture->diamond);
    g_assert (g_list_length (levels) == 3);

    roots = g_list_nth_data (levels, 0);
    g_assert (g_list_length (roots) == 1);
    g_assert (g_list_nth_data (roots, 0) == fixture->root);
    g_list_free (roots);

    second_level = g_list_nth_data (levels, 1);
    g_assert (g_list_length (second_level) == 2);
    g_assert (g_list_find (second_level, fixture->target1) != NULL);
    g_assert (g_list_find (second_level, fixture->target2) != NULL);
    g_list_free (second_level);

    third_level = g_list_nth_data (levels, 2);
    g_assert (g_list_length (third_level) == 1);
    g_assert (g_list_find (third_level, fixture->target3) != NULL);
    g_list_free (third_level);

    g_list_free (levels);
}

void
test_add_graph (void)
{
    TestCase test_cases[] = {
        { "/no-opencl/graph/connected",               test_connected },
        { "/no-opencl/graph/nodes/number",            test_get_num_nodes },
        { "/no-opencl/graph/nodes/roots",             test_get_roots },
        { "/no-opencl/graph/nodes/successors",        test_get_successors },
        { "/no-opencl/graph/nodes/successors/num",    test_get_num_successors },
        { "/no-opencl/graph/nodes/predecessors",      test_get_predecessors },
        { "/no-opencl/graph/nodes/predecessors/num",  test_get_num_predecessors },
        { "/no-opencl/graph/nodes/filtered",          test_get_nodes_filtered },
        { "/no-opencl/graph/edges/number",            test_get_num_edges },
        { "/no-opencl/graph/edges/all",               test_get_edges },
        { "/no-opencl/graph/edges/remove",            test_remove_edge },
        { "/no-opencl/graph/labels",                  test_get_labels },
        { "/no-opencl/graph/expansion",               test_expansion },
        { "/no-opencl/graph/copy",                    test_copy },
        { "/no-opencl/graph/flatten",                 test_flatten },
        { NULL, NULL }
    };

    for (guint i = 0; test_cases[i].path != NULL; i++) {
        g_test_add (test_cases[i].path, Fixture, NULL,
                    fixture_setup, test_cases[i].test_func, fixture_teardown);
    }
}
