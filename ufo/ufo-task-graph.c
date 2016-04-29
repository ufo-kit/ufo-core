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

#include <string.h>
#include <stdio.h>
#include <json-glib/json-glib.h>
#include <ufo/ufo-task-graph.h>
#include <ufo/ufo-task-node.h>
#include <ufo/ufo-remote-node.h>
#include <ufo/ufo-input-task.h>
#include <ufo/ufo-dummy-task.h>
#include <ufo/ufo-remote-task.h>
#include "compat.h"

/**
 * SECTION:ufo-task-graph
 * @Short_description: Hold task nodes
 * @Title: UfoTaskGraph
 */

G_DEFINE_TYPE (UfoTaskGraph, ufo_task_graph, UFO_TYPE_GRAPH)

#define UFO_TASK_GRAPH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_TASK_GRAPH, UfoTaskGraphPrivate))

struct _UfoTaskGraphPrivate {
    UfoPluginManager *manager;
    GHashTable *prop_sets;
    GHashTable *json_nodes;
    GList *remote_tasks;
    guint index;
    guint total;
};

typedef enum {
    JSON_FILE,
    JSON_DATA
} JsonLocation;

static void add_nodes_from_json     (UfoTaskGraph *self, JsonNode *, GError **);
static void handle_json_prop_set    (JsonObject *, const gchar *, JsonNode *, gpointer user);
static void handle_json_task_edge   (JsonArray *, guint, JsonNode *, gpointer);
static void add_task_node_to_json_array (UfoTaskNode *, JsonArray *);
static JsonObject *json_object_from_ufo_node (UfoNode *node);
static JsonNode *get_json_representation (UfoTaskGraph *, GError **);
static UfoTaskNode *create_node_from_json (JsonNode *json_node,UfoPluginManager *manager, GHashTable *prop_sets, GError **error);
static void add_node_to_table (JsonObject *object, const gchar *name, JsonNode *node, gpointer table);

/*
 * ChangeLog:
 * - 1.1: Add "index" and "total" keys to the root object
 */
static const gchar *JSON_API_VERSION = "1.1";

/**
 * UfoTaskGraphError:
 * @UFO_TASK_GRAPH_ERROR_JSON_KEY: Key is not found in JSON
 * @UFO_TASK_GRAPH_ERROR_BAD_INPUTS: Inputs of a task do not play well with each
 *  other.
 *
 * Task graph errors
 */
GQuark
ufo_task_graph_error_quark (void)
{
    return g_quark_from_static_string ("ufo-task-graph-error-quark");
}

/**
 * ufo_task_graph_new:
 *
 * Create a new task graph without any nodes.
 *
 * Returns: A #UfoGraph that can be upcast to a #UfoTaskGraph.
 */
UfoGraph *
ufo_task_graph_new (void)
{
    UfoTaskGraph *graph;
    graph = UFO_TASK_GRAPH (g_object_new (UFO_TYPE_TASK_GRAPH, NULL));
    return UFO_GRAPH (graph);
}

static void
read_json (UfoTaskGraph *graph,
           UfoPluginManager *manager,
           JsonLocation location,
           const gchar *data,
           GError **error)
{
    JsonParser *json_parser;
    JsonNode *json_root;
    JsonObject *object;
    GError *tmp_error = NULL;

    json_parser = json_parser_new ();

    switch (location) {
        case JSON_FILE:
            json_parser_load_from_file (json_parser,
                                        data,
                                        &tmp_error);
            break;

        case JSON_DATA:
            json_parser_load_from_data (json_parser,
                                        data,
                                        (gssize) strlen (data),
                                        &tmp_error);
            break;
    }

    if (tmp_error != NULL) {
        g_propagate_prefixed_error (error, tmp_error, "Parsing JSON: ");
        g_object_unref (json_parser);
        return;
    }

    graph->priv->manager = manager;
    g_object_ref (manager);

    json_root = json_parser_get_root (json_parser);
    object = json_node_get_object (json_root);

    if (json_object_has_member (object, "index") &&
        json_object_has_member (object, "total")) {
        guint index = (guint) json_object_get_int_member (object, "index");
        guint total = (guint) json_object_get_int_member (object, "total");
        ufo_task_graph_set_partition (graph, index, total);
    }

    add_nodes_from_json (graph, json_root, error);
    g_object_unref (json_parser);
}

/**
 * ufo_task_graph_read_from_file:
 * @graph: A #UfoTaskGraph.
 * @manager: A #UfoPluginManager used to load the filters
 * @filename: Path and filename to the JSON file
 * @error: Indicates error in case of failed file loading or parsing
 *
 * Read a JSON configuration file to fill the structure of @graph.
 */
void
ufo_task_graph_read_from_file (UfoTaskGraph *graph,
                               UfoPluginManager *manager,
                               const gchar *filename,
                               GError **error)
{
    g_return_if_fail (UFO_IS_TASK_GRAPH (graph) &&
                      UFO_IS_PLUGIN_MANAGER (manager) &&
                      (filename != NULL));

    read_json (graph, manager, JSON_FILE, filename, error);
}

/**
 * ufo_task_graph_read_from_data:
 * @graph: A #UfoTaskGraph.
 * @manager: A #UfoPluginManager used to load the filters
 * @json: %NULL-terminated string with JSON data
 * @error: Indicates error in case of failed file loading or parsing
 *
 * Read a JSON configuration file to fill the structure of @graph.
 */
void
ufo_task_graph_read_from_data (UfoTaskGraph *graph,
                               UfoPluginManager *manager,
                               const gchar *json,
                               GError **error)
{
    g_return_if_fail (UFO_IS_TASK_GRAPH (graph) &&
                      UFO_IS_PLUGIN_MANAGER (manager) &&
                      (json != NULL));

    read_json (graph, manager, JSON_DATA, json, error);
}

static JsonNode *
get_json_representation (UfoTaskGraph *graph,
                         GError **error)
{
    GList *task_nodes;
    GList *it;
    JsonNode *root_node = json_node_new (JSON_NODE_OBJECT);
    JsonObject *root_object = json_object_new ();
    JsonArray *nodes = json_array_new ();
    JsonArray *edges = json_array_new ();

    task_nodes = ufo_graph_get_nodes (UFO_GRAPH (graph));
    g_list_foreach (task_nodes, (GFunc) add_task_node_to_json_array, nodes);

    g_list_for (task_nodes, it) {
        UfoNode *from;
        GList *successors;
        GList *jt;

        from = UFO_NODE (it->data);
        successors = ufo_graph_get_successors (UFO_GRAPH (graph), from);

        g_list_for (successors, jt) {
            UfoNode *to;
            gint port;
            JsonObject *to_object;
            JsonObject *from_object;
            JsonObject *edge_object;

            to = UFO_NODE (jt->data);
            port = GPOINTER_TO_INT (ufo_graph_get_edge_label (UFO_GRAPH (graph), from, to));
            to_object  = json_object_from_ufo_node (to);
            from_object = json_object_from_ufo_node (from);
            edge_object = json_object_new ();

            json_object_set_int_member (to_object, "input", port);
            json_object_set_object_member (edge_object, "to", to_object);
            json_object_set_object_member (edge_object, "from", from_object);
            json_array_add_object_element (edges, edge_object);
        }

        g_list_free (successors);
    }

    json_object_set_string_member (root_object, "version", JSON_API_VERSION);
    json_object_set_array_member (root_object, "nodes", nodes);
    json_object_set_array_member (root_object, "edges", edges);
    json_object_set_int_member (root_object, "index", graph->priv->index);
    json_object_set_int_member (root_object, "total", graph->priv->total);

    json_node_set_object (root_node, root_object);
    g_list_free (task_nodes);

    return root_node;
}

static JsonGenerator *
task_graph_to_generator (UfoTaskGraph *graph,
                         GError **error)
{
    JsonNode *root_node;
    JsonGenerator *generator;

    root_node = get_json_representation (graph, error);

    if (error != NULL && *error != NULL)
        return NULL;

    generator = json_generator_new ();
    json_generator_set_root (generator, root_node);
    json_node_free (root_node);

    return generator;
}

/**
 * ufo_task_graph_save_to_json:
 * @graph: A #UfoTaskGraph.
 * @filename: Path and filename to the JSON file
 * @error: Indicates error in case of failed file saving
 *
 * Save a JSON configuration file with the filter structure of @graph.
 */
void
ufo_task_graph_save_to_json (UfoTaskGraph *graph,
                             const gchar *filename,
                             GError **error)
{
    JsonGenerator *generator;

    generator = task_graph_to_generator (graph, error);

    if (generator != NULL) {
        json_generator_to_file (generator, filename, error);
        g_object_unref (generator);
    }
}

gchar *
ufo_task_graph_get_json_data (UfoTaskGraph *graph,
                              GError **error)
{
    JsonGenerator *generator;
    gchar *json = NULL;

    generator = task_graph_to_generator (graph, error);

    if (generator != NULL) {
        json = json_generator_to_data (generator, NULL);
        g_object_unref (generator);
    }

    return json;
}

static gboolean
is_gpu_task (UfoTask *task, gpointer user_data)
{
    return ufo_task_uses_gpu (task);
}

static UfoTaskNode *
build_remote_graph (UfoTaskGraph *remote_graph,
                    GList *first,
                    GList *last)
{
    UfoTaskNode *node = NULL;
    UfoTaskNode *predecessor = NULL;

    for (GList *it = g_list_next (first); it != last; it = g_list_next (it)) {
        node = UFO_TASK_NODE (it->data);

        if (predecessor != NULL)
            ufo_task_graph_connect_nodes (remote_graph, predecessor, node);

        predecessor = node;
    }

    g_assert (node != NULL);
    return node;
}

static void
create_remote_tasks (UfoTaskGraph *task_graph,
                     UfoTaskGraph *remote_graph,
                     UfoTaskNode *first,
                     UfoTaskNode *last,
                     UfoRemoteNode *remote)
{
    UfoTaskGraphPrivate *priv;
    UfoTaskNode *task;
    gchar *json;

    json = ufo_task_graph_get_json_data (remote_graph, NULL);
    priv = task_graph->priv;
    ufo_remote_node_send_json (remote, UFO_REMOTE_MODE_STREAM, json);

    task = UFO_TASK_NODE (ufo_remote_task_new ());
    priv->remote_tasks = g_list_append (priv->remote_tasks, task);
    ufo_task_node_set_proc_node (task, UFO_NODE (remote));

    ufo_task_graph_connect_nodes (task_graph, first, task);
    ufo_task_graph_connect_nodes (task_graph, task, last);

    g_debug ("remote: connected %s -> [remote] -> %s",
             ufo_task_node_get_identifier (first),
             ufo_task_node_get_identifier (last));

    g_free (json);
}

static void
expand_remotes (UfoTaskGraph *task_graph,
                GList *remotes,
                GList *path)
{
    UfoTaskGraph *remote_graph;
    UfoTaskNode *node;
    GList *first;
    GList *last;
    GList *it;

    first = g_list_first (path);
    last = g_list_last (path);

    remote_graph = UFO_TASK_GRAPH (ufo_task_graph_new ());
    node = build_remote_graph (remote_graph, first, last);

    if (ufo_graph_get_num_nodes (UFO_GRAPH (remote_graph)) == 0) {
        ufo_task_graph_connect_nodes (remote_graph, UFO_TASK_NODE (ufo_dummy_task_new ()), node);
    }

    g_list_for (remotes, it) {
        create_remote_tasks (task_graph, remote_graph,
                             first->data, last->data, it->data);
    }

    g_object_unref (remote_graph);
}

static gboolean
has_common_ancestries (UfoTaskGraph *graph, GList *path)
{
    GList *it;

    g_list_for (path, it) {
        if (ufo_graph_get_num_predecessors (UFO_GRAPH (graph), UFO_NODE (it->data)) > 1)
            return TRUE;
    }

    return FALSE;
}

/**
 * ufo_task_graph_expand:
 * @task_graph: A #UfoTaskGraph
 * @resources: A #UfoResources objects
 * @n_gpus: Number of GPUs to expand the graph for
 * @expand_remote: %TRUE if remote nodes should be inserted
 *
 * Expands @task_graph in a way that most of the resources in @arch_graph can be
 * occupied. In the simple pipeline case, the longest possible GPU paths are
 * duplicated as much as there are GPUs in @arch_graph.
 */
void
ufo_task_graph_expand (UfoTaskGraph *task_graph,
                       UfoResources *resources,
                       guint n_gpus,
                       gboolean expand_remote)
{
    GList *path;

    g_return_if_fail (UFO_IS_TASK_GRAPH (task_graph));

    path = ufo_graph_find_longest_path (UFO_GRAPH (task_graph), (UfoFilterPredicate) is_gpu_task, NULL);

    /* Check if any node on the path contains multiple inputs and stop expansion */
    /* TODO: we need a better strategy at this point */
    if (has_common_ancestries (task_graph, path)) {
        g_list_free (path);
        return;
    }

    if (path != NULL && g_list_length (path) > 0) {
        GList *predecessors;
        GList *successors;

        g_object_unref (UFO_NODE (g_list_first (path)->data));

        if (g_list_length (path) > 1)
            g_object_unref (UFO_NODE (g_list_last (path)->data));

        /* Add predecessor and successor nodes to path */
        predecessors = ufo_graph_get_predecessors (UFO_GRAPH (task_graph),
                                                   UFO_NODE (g_list_first (path)->data));

        successors = ufo_graph_get_successors (UFO_GRAPH (task_graph),
                                               UFO_NODE (g_list_last (path)->data));
        
        if (predecessors != NULL)
            path = g_list_prepend (path, g_list_first (predecessors)->data);

        if (successors != NULL)
            path = g_list_append (path, g_list_first (successors)->data);

        g_list_free (predecessors);
        g_list_free (successors);

        if (expand_remote) {
            GList *remotes;
            guint n_remotes;

            remotes = ufo_resources_get_remote_nodes (resources);
            n_remotes = g_list_length (remotes);

            if (n_remotes > 0) {
                g_debug ("Expand for %i remote nodes", n_remotes);
                expand_remotes (task_graph, remotes, path);
            }

            g_list_free (remotes);
        }

        g_debug ("Expand for %i GPU nodes", n_gpus);

        for (guint i = 1; i < n_gpus; i++)
            ufo_graph_expand (UFO_GRAPH (task_graph), path);
    }

    g_list_free (path);
}

/**
 * ufo_task_graph_fuse:
 * @task_graph: A #UfoTaskGraph
 *
 * Fuses task nodes to increase data locality.
 *
 * Note: This is not implemented and a no-op right now.
 */
void
ufo_task_graph_fuse (UfoTaskGraph *task_graph)
{
}

static void
map_proc_node (UfoGraph *graph,
               UfoNode *node,
               guint proc_index,
               GList *gpu_nodes)
{
    UfoNode *proc_node;
    GList *successors;
    GList *it;
    guint n_gpus;

    proc_node = UFO_NODE (g_list_nth_data (gpu_nodes, proc_index));

    if ((ufo_task_uses_gpu (UFO_TASK (node)) || UFO_IS_INPUT_TASK (node)) &&
        (!ufo_task_node_get_proc_node (UFO_TASK_NODE (node)))) {

        g_debug ("Mapping UfoGpuNode-%p to %s-%p",
                 (gpointer) proc_node,
                 G_OBJECT_TYPE_NAME (node), (gpointer) node);

        ufo_task_node_set_proc_node (UFO_TASK_NODE (node), proc_node);
    }

    n_gpus = g_list_length (gpu_nodes);
    successors = ufo_graph_get_successors (graph, node);

    g_list_for (successors, it) {
        map_proc_node (graph, UFO_NODE (it->data), proc_index, gpu_nodes);

        if (!UFO_IS_REMOTE_TASK (UFO_NODE (it->data)))
            proc_index = n_gpus > 0 ? (proc_index + 1) % n_gpus : 0;
    }

    g_list_free (successors);
}

/**
 * ufo_task_graph_is_alright:
 * @task_graph: A #UfoTaskGraph
 * @error: Location for a GError or %NULL
 *
 * Check if nodes int the task graph are properly connected.
 *
 * Returns: %TRUE if everything is alright, %FALSE else.
 */
gboolean
ufo_task_graph_is_alright (UfoTaskGraph *task_graph,
                           GError **error)
{
    GList *nodes;
    GList *it;
    gboolean alright = TRUE;

    nodes = ufo_graph_get_nodes (UFO_GRAPH (task_graph));

    /* Check that nodes don't receive input from reductors and processors */
    g_list_for (nodes, it) {
        GList *predecessors;

        predecessors = ufo_graph_get_predecessors (UFO_GRAPH (task_graph), UFO_NODE (it->data));

        if (g_list_length (predecessors) > 1) {
            GList *jt;
            UfoTaskMode combined_modes = UFO_TASK_MODE_INVALID;

            g_list_for (predecessors, jt)
                combined_modes |= ufo_task_get_mode (UFO_TASK (jt->data));

            if ((combined_modes & UFO_TASK_MODE_PROCESSOR) && (combined_modes & UFO_TASK_MODE_REDUCTOR)) {
#if 0
                g_set_error (error, UFO_TASK_GRAPH_ERROR, UFO_TASK_GRAPH_ERROR_BAD_INPUTS,
                             "`%s' receives both processor and reductor inputs which may deadlock.",
                             ufo_task_node_get_plugin_name (UFO_TASK_NODE (it->data)));
                g_list_free (predecessors);
                g_list_free (nodes);
                return FALSE;
#endif
                g_warning ("`%s' receives both processor and reductor inputs which may deadlock.",
                           ufo_task_node_get_plugin_name (UFO_TASK_NODE (it->data)));
            }
        }

        g_list_free (predecessors);
    }

    g_list_free (nodes);

    /* Check leaves are sinks */
    nodes = ufo_graph_get_leaves (UFO_GRAPH (task_graph));

    g_list_for (nodes, it) {
        if ((ufo_task_get_mode (UFO_TASK (it->data)) & UFO_TASK_MODE_TYPE_MASK) != UFO_TASK_MODE_SINK) {
            alright = FALSE;
            g_set_error (error, UFO_TASK_GRAPH_ERROR, UFO_TASK_GRAPH_ERROR_BAD_INPUTS,
                         "`%s' is a leaf node but not a sink task",
                         ufo_task_node_get_plugin_name (UFO_TASK_NODE (it->data)));
            break;
        }
    }

    g_list_free (nodes);

    return alright;
}

/**
 * ufo_task_graph_map:
 * @task_graph: A #UfoTaskGraph
 * @gpu_nodes: (transfer none) (element-type Ufo.GpuNode): List of #UfoGpuNode objects
 *
 * Map task nodes of @task_graph to the processing nodes of @arch_graph. Not
 * doing this could break execution of @task_graph.
 */
void
ufo_task_graph_map (UfoTaskGraph *task_graph,
                    GList *gpu_nodes)
{
    GList *roots;
    GList *it;

    roots = ufo_graph_get_roots (UFO_GRAPH (task_graph));

    g_list_for (roots, it) {
        map_proc_node (UFO_GRAPH (task_graph), UFO_NODE (it->data), 0, gpu_nodes);
    }

    g_list_free (roots);
}

/**
 * ufo_task_graph_connect_nodes:
 * @graph: A #UfoTaskGraph
 * @n1: A source node
 * @n2: A destination node
 *
 * Connect @n1 with @n2 using @n2's default input port. To specify any other
 * port, use ufo_task_graph_connect_nodes_full().
 */
void
ufo_task_graph_connect_nodes (UfoTaskGraph *graph,
                              UfoTaskNode *n1,
                              UfoTaskNode *n2)
{
    ufo_task_graph_connect_nodes_full (graph, n1, n2, 0);
}

/**
 * ufo_task_graph_connect_nodes_full:
 * @graph: A #UfoTaskGraph
 * @n1: A source node
 * @n2: A destination node
 * @input: Input port of @n2
 *
 * Connect @n1 with @n2 using @n2's @input port.
 */
void
ufo_task_graph_connect_nodes_full (UfoTaskGraph *graph,
                                   UfoTaskNode *n1,
                                   UfoTaskNode *n2,
                                   guint input)
{
    ufo_graph_connect_nodes (UFO_GRAPH (graph), UFO_NODE (n1), UFO_NODE (n2), GINT_TO_POINTER (input));
}

void
ufo_task_graph_set_partition (UfoTaskGraph *graph,
                              guint index,
                              guint total)
{
    g_return_if_fail (UFO_IS_TASK_GRAPH (graph));
    g_assert (index < total);
    graph->priv->index = index;
    graph->priv->total = total;
}

void
ufo_task_graph_get_partition (UfoTaskGraph *graph,
                              guint *index,
                              guint *total)
{
    g_return_if_fail (UFO_IS_TASK_GRAPH (graph));
    *index = graph->priv->index;
    *total = graph->priv->total;
}

static void
add_nodes_from_json (UfoTaskGraph *self,
                     JsonNode *root,
                     GError **error)
{
    UfoTaskGraphPrivate *priv = UFO_TASK_GRAPH_GET_PRIVATE(self);
    JsonObject *root_object = json_node_get_object (root);

    if (json_object_has_member (root_object, "prop-sets")) {
        JsonObject *sets = json_object_get_object_member (root_object, "prop-sets");
        json_object_foreach_member (sets, handle_json_prop_set, priv);
    }

    if (json_object_has_member (root_object, "nodes")) {
        JsonArray *nodes = json_object_get_array_member (root_object, "nodes");
        GList *elements = json_array_get_elements (nodes);
        GList *it;

        g_list_for (elements, it) {
            UfoTaskNode *new_node = create_node_from_json (it->data, priv->manager, priv->prop_sets, error);
            if(new_node != NULL) {
                const char *name = ufo_task_node_get_identifier(new_node);

                if (g_hash_table_lookup (priv->json_nodes, name) != NULL) {
                    g_error ("Duplicate name `%s' found", name);
                }

                g_hash_table_insert (priv->json_nodes, g_strdup (name), new_node);
            }
            else {
                g_list_free (elements);
                return;
            }
        }
        g_list_free (elements);

        /*
         * We only check edges if we have nodes, anything else doesn't make much
         * sense.
         */
        if (json_object_has_member (root_object, "edges")) {
            JsonArray *edges = json_object_get_array_member (root_object, "edges");
            json_array_foreach_element (edges, handle_json_task_edge, self);
        }
    }
}

static UfoTaskNode *
create_node_from_json (JsonNode *json_node,
                       UfoPluginManager *manager,
                       GHashTable *prop_sets,
                       GError **error)
{
    UfoTaskNode *ret_node = NULL;
    JsonObject *json_object;
    GError *tmp_error = NULL;
    GHashTableIter iter;
    gpointer key, value;
    const gchar *plugin_name;
    const gchar *task_name;

    json_object = json_node_get_object (json_node);

    if (!json_object_has_member (json_object, "plugin") ||
        !json_object_has_member (json_object, "name")) {
        g_set_error (error, UFO_TASK_GRAPH_ERROR, UFO_TASK_GRAPH_ERROR_JSON_KEY,
                     "Node does not have `plugin' or `name' key");
        return NULL;
    }

    plugin_name = json_object_get_string_member (json_object, "plugin");

    if (json_object_has_member (json_object, "package")) {
        const gchar *package_name = json_object_get_string_member (json_object, "package");
        ret_node = ufo_plugin_manager_get_task_from_package (manager, package_name, plugin_name, &tmp_error);
    }
    else {
        ret_node = ufo_plugin_manager_get_task (manager, plugin_name, &tmp_error);
    }

    ufo_task_node_set_plugin_name (ret_node, plugin_name);

    if (tmp_error != NULL) {
        g_propagate_error (error, tmp_error);
        return NULL;
    }

    task_name = json_object_get_string_member (json_object, "name");
    ufo_task_node_set_identifier (ret_node, task_name);

    GHashTable *nodes_to_process = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    if (json_object_has_member (json_object, "properties")) {
        JsonObject *prop_object = json_object_get_object_member (json_object, "properties");
        json_object_foreach_member (prop_object, add_node_to_table, nodes_to_process);
    }

    if (json_object_has_member (json_object, "prop-refs")) {
        JsonArray *prop_refs = json_object_get_array_member (json_object, "prop-refs");

        for (guint i = 0; i < json_array_get_length (prop_refs); i++) {
            const gchar *ref_name = json_array_get_string_element (prop_refs, i);
            JsonObject *prop_set = g_hash_table_lookup (prop_sets, ref_name);

            if (prop_set == NULL) {
                g_warning ("No property set `%s' found in `prop-sets'", ref_name);
            }
            else {
                json_object_foreach_member (prop_set, add_node_to_table, nodes_to_process);
            }
        }
    }

    g_hash_table_iter_init (&iter, nodes_to_process);

    while (g_hash_table_iter_next (&iter, &key, &value)) {
        JsonNode *node = (JsonNode *) value;

        if (JSON_NODE_HOLDS_VALUE (node)) {
            GValue val = {0,};
            json_node_get_value (node, &val);
            g_object_set_property (G_OBJECT(ret_node), key, &val);
            g_value_unset (&val);
        }
        else if (JSON_NODE_HOLDS_OBJECT (node)) {
            JsonObject *node_object = json_node_get_object (node);

            if (json_object_has_member (node_object, "plugin")) {
                UfoTaskNode *inner_node = create_node_from_json (node, manager, prop_sets, error);
                g_object_force_floating (G_OBJECT (inner_node));
                g_object_set (G_OBJECT (ret_node), key, inner_node, NULL);
            }
            else {
                ufo_task_set_json_object_property (UFO_TASK (node), key, node_object);
            }
        }
        else {
            g_warning ("`%s' is neither a primitive value nor an object!", (char*)key);
        }
    }

    g_hash_table_unref (nodes_to_process);
    return ret_node;
}

static void
handle_json_task_edge (JsonArray *array,
                       guint index,
                       JsonNode *element,
                       gpointer user)
{
    UfoTaskGraph *graph = user;
    UfoTaskGraphPrivate *priv = graph->priv;
    JsonObject *edge;
    UfoTaskNode *from_node, *to_node;
    JsonObject *from_object, *to_object;
    guint to_port;
    const gchar *from_name;
    const gchar *to_name;
    GError *error = NULL;

    edge = json_node_get_object (element);

    if (!json_object_has_member (edge, "from") ||
        !json_object_has_member (edge, "to")) {
        g_error ("Edge does not have `from' or `to' key");
        return;
    }

    /* Get from details */
    from_object = json_object_get_object_member (edge, "from");

    if (!json_object_has_member (from_object, "name")) {
        g_error ("From node does not have `name' key");
        return;
    }

    from_name = json_object_get_string_member (from_object, "name");

    /* Get to details */
    to_object = json_object_get_object_member (edge, "to");

    if (!json_object_has_member (to_object, "name")) {
        g_error ("To node does not have `name' key");
        return;
    }

    to_name = json_object_get_string_member (to_object, "name");
    to_port = 0;

    if (json_object_has_member (to_object, "input"))
        to_port = (guint) json_object_get_int_member (to_object, "input");

    /* Get actual filters and connect them */
    from_node = g_hash_table_lookup (priv->json_nodes, from_name);
    to_node = g_hash_table_lookup (priv->json_nodes, to_name);

    if (from_node == NULL)
        g_error ("No filter `%s' defined", from_name);

    if (to_node == NULL)
        g_error ("No filter `%s' defined", to_name);

    ufo_task_graph_connect_nodes_full (graph, from_node, to_node, to_port);

    if (error != NULL)
        g_warning ("%s", error->message);
}

static void
handle_json_prop_set (JsonObject *object,
                      const gchar *name,
                      JsonNode *node,
                      gpointer user)
{
    UfoTaskGraphPrivate *priv;
    JsonObject *properties;

    priv = (UfoTaskGraphPrivate *) user;
    properties = json_object_get_object_member (object, name);
    json_object_ref (properties);
    g_hash_table_insert (priv->prop_sets, g_strdup (name), properties);
}

static void
add_node_to_table (JsonObject *object, const gchar *name, JsonNode *node, gpointer table)
{
    GHashTable *hash_table = (GHashTable*)table;
    g_hash_table_insert(hash_table, g_strdup(name), node);
}

static JsonObject *
create_full_json_from_task_node (UfoTaskNode *task_node)
{
    JsonObject *node_object;
    JsonNode *prop_node;
    JsonObject *prop_object;
    GParamSpec **pspecs;
    guint n_pspecs;
    const gchar *plugin_name;
    const gchar *package_name;
    const gchar *name;
        
    node_object = json_object_new ();
    plugin_name = ufo_task_node_get_plugin_name (task_node);

    /* plugin_name can be NULL for task graphs expanded with remote nodes */
    if (plugin_name == NULL)
        return NULL;

    json_object_set_string_member (node_object, "plugin", plugin_name);

    package_name = ufo_task_node_get_package_name (task_node);

    if (package_name != NULL)
        json_object_set_string_member (node_object, "package", package_name);

    name = ufo_task_node_get_identifier (task_node);
    g_assert (name != NULL);
    json_object_set_string_member (node_object, "name", name);

    prop_node = json_gobject_serialize (G_OBJECT (task_node));

    /* Remove num-processed which is a read-only property */
    json_object_remove_member (json_node_get_object (prop_node), "num-processed");

    prop_object = json_node_get_object (prop_node);
    pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (task_node), &n_pspecs);

    for (guint i = 0; i < n_pspecs; i++) {
        GParamSpec *pspec = pspecs[i];
        GValue value = { 0, };

        /* read only what we can */
        if (!(pspec->flags & G_PARAM_READABLE))
            continue;

        /* Process only task node type properties */
        if (!UFO_IS_TASK_NODE_CLASS (&(pspec->value_type)))
            continue;

        g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
        g_object_get_property (G_OBJECT (task_node), pspec->name, &value);

        /* skip if the value is the default for the property */
        if (!g_param_value_defaults (pspec, &value)) {
            JsonObject *subtask_json_object = create_full_json_from_task_node (g_value_get_object (&value));
            json_object_set_object_member (prop_object, pspec->name, subtask_json_object);
        }

        g_value_unset (&value);
    }

    g_free (pspecs);
    json_object_set_member (node_object, "properties", prop_node);

    return node_object;
}

static void
add_task_node_to_json_array (UfoTaskNode *node, JsonArray *array)
{
    json_array_add_object_element (array, create_full_json_from_task_node (node));
}

static JsonObject *
json_object_from_ufo_node (UfoNode *node)
{
    JsonObject *object;

    object = json_object_new ();
    const gchar *unique_name = ufo_task_node_get_identifier (UFO_TASK_NODE (node));
    json_object_set_string_member (object, "name", unique_name);
    return object;
}

static void
ufo_task_graph_dispose (GObject *object)
{
    UfoTaskGraphPrivate *priv;
    GList *nodes;

    priv = UFO_TASK_GRAPH_GET_PRIVATE (object);

    if (priv->manager != NULL) {
        g_object_unref (priv->manager);
        priv->manager = NULL;
    }

    g_list_foreach (priv->remote_tasks, (GFunc) g_object_unref, NULL);
    g_list_free (priv->remote_tasks);
    priv->remote_tasks = NULL;

    nodes = g_hash_table_get_values (priv->json_nodes);
    g_list_foreach (nodes, (GFunc) g_object_unref, NULL);
    g_list_free (nodes);

    G_OBJECT_CLASS (ufo_task_graph_parent_class)->dispose (object);
}

static void
ufo_task_graph_finalize (GObject *object)
{
    UfoTaskGraphPrivate *priv;

    priv = UFO_TASK_GRAPH_GET_PRIVATE (object);

    g_hash_table_destroy (priv->json_nodes);
    g_hash_table_destroy (priv->prop_sets);

    G_OBJECT_CLASS (ufo_task_graph_parent_class)->finalize (object);
}

static void
ufo_task_graph_class_init (UfoTaskGraphClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);
    oclass->dispose = ufo_task_graph_dispose;
    oclass->finalize = ufo_task_graph_finalize;

    g_type_class_add_private(klass, sizeof(UfoTaskGraphPrivate));
}

static void
ufo_task_graph_init (UfoTaskGraph *self)
{
    UfoTaskGraphPrivate *priv;
    self->priv = priv = UFO_TASK_GRAPH_GET_PRIVATE (self);

    priv->manager = NULL;
    priv->remote_tasks = NULL;

    priv->json_nodes = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              g_free, NULL);

    priv->prop_sets = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             g_free, (GDestroyNotify) json_object_unref);
    priv->index = 0;
    priv->total = 1;
}
