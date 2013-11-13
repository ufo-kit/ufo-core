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
#include <json-glib/json-glib.h>
#include <ufo/ufo-task-graph.h>
#include <ufo/ufo-task-node.h>
#include <ufo/ufo-remote-node.h>
#include <ufo/ufo-cpu-task-iface.h>
#include <ufo/ufo-gpu-task-iface.h>
#include <ufo/ufo-input-task.h>
#include <ufo/ufo-dummy-task.h>
#include <ufo/ufo-remote-task.h>

/**
 * SECTION:ufo-task-graph
 * @Short_description: Hold and manage #UfoTaskNode elements.
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

static void add_nodes_from_json     (UfoTaskGraph *, JsonNode *, GError **);
static void handle_json_prop_set    (JsonObject *, const gchar *, JsonNode *, gpointer user);
static void handle_json_single_prop (JsonObject *, const gchar *, JsonNode *, gpointer user);
static void handle_json_task_edge   (JsonArray *, guint, JsonNode *, gpointer);
static gboolean handle_json_task_node (JsonNode *, UfoTaskGraphPrivate *priv, GError **error);
static void add_task_node_to_json_array (UfoTaskNode *, JsonArray *);
static JsonObject *json_object_from_ufo_node (UfoNode *node);
static JsonNode *get_json_representation (UfoTaskGraph *, GError **);

/*
 * ChangeLog:
 * - 1.1: Add "index" and "total" keys to the root object
 */
static const gchar *JSON_API_VERSION = "1.1";

/**
 * UfoTaskGraphError:
 * @UFO_TASK_GRAPH_ERROR_JSON_KEY: Key is not found in JSON
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
    else {
        g_warning ("JSON does not define `index' and `total' keys");
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
    JsonNode *root_node = json_node_new (JSON_NODE_OBJECT);
    JsonObject *root_object = json_object_new ();
    JsonArray *nodes = json_array_new ();
    JsonArray *edges = json_array_new ();

    task_nodes = ufo_graph_get_nodes (UFO_GRAPH (graph));
    g_list_foreach (task_nodes, (GFunc) add_task_node_to_json_array, nodes);

    for (GList *it = g_list_first (task_nodes); it != NULL; it = g_list_next (it)) {
        UfoNode *from;
        GList *successors;

        from = UFO_NODE (it->data);
        successors = ufo_graph_get_successors (UFO_GRAPH (graph), from);

        for (GList *jt = g_list_first (successors); jt != NULL; jt = g_list_next (jt)) {
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
is_gpu_task (UfoNode *node, gpointer user_data)
{
    return UFO_IS_GPU_TASK (node);
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

    priv = task_graph->priv;
    json = ufo_task_graph_get_json_data (remote_graph, NULL);
    ufo_remote_node_send_json (remote, UFO_REMOTE_MODE_STREAM, json);

    task = UFO_TASK_NODE (ufo_remote_task_new ());
    priv->remote_tasks = g_list_append (priv->remote_tasks, task);
    ufo_task_node_set_proc_node (task, UFO_NODE (remote));

    ufo_task_graph_connect_nodes (task_graph, first, task);
    ufo_task_graph_connect_nodes (task_graph, task, last);

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

    first = g_list_first (path);
    last = g_list_last (path);
    remote_graph = UFO_TASK_GRAPH (ufo_task_graph_new ());
    node = build_remote_graph (remote_graph, first, last);

    if (ufo_graph_get_num_nodes (UFO_GRAPH (remote_graph)) == 0) {
        ufo_task_graph_connect_nodes (remote_graph,
                                      UFO_TASK_NODE (ufo_dummy_task_new ()),
                                      node);
    }

    for (GList *jt = g_list_first (remotes); jt != NULL; jt = g_list_next (jt)) {
        create_remote_tasks (task_graph, remote_graph,
                             first->data, last->data, jt->data);
    }

    g_object_unref (remote_graph);
}

static gboolean
path_unvisited (GList *path,
                GList **visited)
{
    GList *head;
    GList *tail;

    head = g_list_first (path);
    tail = g_list_last (path);

    for (GList *it = g_list_first (head); it != tail; it = g_list_next (it)) {
        UfoNode *node = (UfoNode *) it->data;

        if (g_list_find (*visited, node))
            return FALSE;

        *visited = g_list_append (*visited, node);
    }

    return TRUE;
}

static GList *
remove_common_ancestry_paths (GList *paths)
{
    GList *result;
    GList *visited;

    result = NULL;
    visited = NULL;

    for (GList *it = g_list_first (paths); it != NULL; it = g_list_next (it)) {
        GList *path = (GList *) it->data;

        if (path_unvisited (it->data, &visited))
            result = g_list_append (result, path);
    }

    g_list_free (visited);
    g_list_free (paths);
    return result;
}

static GList *
find_longest_path (GList *paths)
{
    GList *longest = NULL;
    guint max_length = 0;

    for (GList *it = g_list_first (paths); it != NULL; it = g_list_next (it)) {
        guint length;
        GList *path;

        path = (GList *) it->data;
        length = g_list_length (path);

        if (length > max_length) {
            max_length = length;
            longest = path;
        }
    }

    return longest;
}

static gboolean
is_writer_node (UfoNode *node, gpointer user_data)
{
    const gchar *name = ufo_task_node_get_plugin_name (UFO_TASK_NODE (node));
    if (name == NULL)
        return FALSE;
    return g_str_has_prefix (name, "writer");
}

UfoNode *
ufo_task_graph_get_writer_node (UfoTaskGraph *task_graph)
{
    GList *allnodes = ufo_graph_get_nodes_filtered (UFO_GRAPH (task_graph), is_writer_node, NULL);
    if (allnodes == NULL)
        return NULL;

    return (UfoNode *) g_list_nth_data (allnodes, 0);
}

/**
 * ufo_task_graph_expand:
 * @task_graph: A #UfoTaskGraph
 * @arch_graph: A #UfoArchGraph
 * @expand_remote: %TRUE if remote nodes should be inserted
 * @expand_gpu: %FALSE if the master node should not use gpus for computing
 * @use_network_writer: %TRUE if the last remote node should be used for dedicated
 *                      writing (no computation)
 *
 * Expands @task_graph in a way that most of the resources in @arch_graph can be
 * occupied. In the simple pipeline case, the longest possible GPU paths are
 * duplicated as much as there are GPUs in @arch_graph.
 */
void
ufo_task_graph_expand (UfoTaskGraph *task_graph,
                       UfoArchGraph *arch_graph,
                       gboolean expand_remote,
                       gboolean expand_gpu,
                       gboolean network_writer)
{
    UfoTaskGraphPrivate *priv = UFO_TASK_GRAPH_GET_PRIVATE (task_graph);
    GList *paths;
    GList *path;

    g_return_if_fail (UFO_IS_TASK_GRAPH (task_graph));

    paths = ufo_graph_get_paths (UFO_GRAPH (task_graph), is_gpu_task);
    g_debug ("Number of identified paths: %i", g_list_length (paths));
    paths = remove_common_ancestry_paths (paths);
    g_debug ("Number of cleaned paths: %i", g_list_length (paths));
    path = find_longest_path (paths);

    GList *remotes = ufo_arch_graph_get_remote_nodes (arch_graph);

    UfoRemoteNode *writer_remote = NULL;
    if (network_writer) {
        // last remote reserved for writer
        GList *last_remote = g_list_last (remotes);
        if (last_remote != NULL) {
            writer_remote = UFO_REMOTE_NODE (last_remote->data);
            remotes = g_list_remove (remotes, writer_remote);
        }
    }

    if (path != NULL) {
        guint n_gpus;

        guint n_remotes = g_list_length (remotes);
        if (expand_remote && n_remotes > 0) {
            g_debug ("Expand for %i remote nodes", n_remotes);
            expand_remotes (task_graph, remotes, path);

            g_list_free (remotes);
        }

        if (expand_gpu) {
            n_gpus = ufo_arch_graph_get_num_gpus (arch_graph);
            g_debug ("Expand for %i GPU nodes", n_gpus);

            for (guint i = 1; i < n_gpus; i++)
                ufo_graph_expand (UFO_GRAPH (task_graph), path);
        }

        // only execute on main runner, not on ufod
        if (network_writer && expand_remote && g_list_length (remotes) > 0) {
            // find the writer task
            UfoNode *writer_node = ufo_task_graph_get_writer_node (task_graph);
            g_assert (writer_node != NULL);
            // create a remote node for it
            UfoTaskGraph *remote_graph = UFO_TASK_GRAPH (ufo_task_graph_new ());
            UfoTaskNode *writer_task = UFO_TASK_NODE (writer_node);

            // construct a remote graph that we send to the rmeote (only thewriter node)
            UfoTaskNode *dummy = UFO_TASK_NODE (ufo_dummy_task_new ());
            ufo_task_graph_connect_nodes (remote_graph, dummy, writer_task);
            gchar *json;
            json = ufo_task_graph_get_json_data (remote_graph, NULL);
            ufo_remote_node_send_json (writer_remote, UFO_REMOTE_MODE_STREAM, json);

            // create the mapping RemoteTask<->RemoteNode
            UfoTaskNode *remote_task = UFO_TASK_NODE (ufo_remote_task_new ());
            priv->remote_tasks = g_list_append (priv->remote_tasks, remote_task);
            ufo_task_node_set_proc_node (remote_task, UFO_NODE (writer_remote));

            // remove all edges to the local writer task node
            // re-connect all edges  to the remote task
            ufo_graph_replace_node (UFO_GRAPH (task_graph), UFO_NODE (writer_task), UFO_NODE (remote_task));
        }
    }

    g_list_foreach (paths, (GFunc) g_list_free, NULL);
    g_list_free (paths);
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
    guint n_gpus;

    proc_node = UFO_NODE (g_list_nth_data (gpu_nodes, proc_index));

    if ((UFO_IS_GPU_TASK (node) || UFO_IS_INPUT_TASK (node)) &&
        (!ufo_task_node_get_proc_node (UFO_TASK_NODE (node)))) {

        g_debug ("Mapping GPU %i to %s-%p",
                 proc_index, G_OBJECT_TYPE_NAME (node),
                 (gpointer) node);

        ufo_task_node_set_proc_node (UFO_TASK_NODE (node), proc_node);
    }

    n_gpus = g_list_length (gpu_nodes);
    successors = ufo_graph_get_successors (graph, node);

    for (GList *it = g_list_first (successors); it != NULL; it = g_list_next (it)) {
        map_proc_node (graph, UFO_NODE (it->data), proc_index, gpu_nodes);

        if (!UFO_IS_REMOTE_TASK (UFO_NODE (it->data)))
            proc_index = (proc_index + 1) % n_gpus;
    }

    g_list_free (successors);
}


/**
 * ufo_task_graph_map:
 * @task_graph: A #UfoTaskGraph
 * @arch_graph: A #UfoArchGraph to which @task_graph's nodes are mapped onto
 *
 * Map task nodes of @task_graph to the processing nodes of @arch_graph. Not
 * doing this could break execution of @task_graph.
 */
void
ufo_task_graph_map (UfoTaskGraph *task_graph,
                    UfoArchGraph *arch_graph)
{
    GList *gpu_nodes;
    GList *roots;

    gpu_nodes = ufo_arch_graph_get_gpu_nodes (arch_graph);
    roots = ufo_graph_get_roots (UFO_GRAPH (task_graph));

    for (GList *it = g_list_first (roots); it != NULL; it = g_list_next (it))
        map_proc_node (UFO_GRAPH (task_graph), UFO_NODE (it->data), 0, gpu_nodes);

    g_list_free (roots);
    g_list_free (gpu_nodes);
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
add_nodes_from_json (UfoTaskGraph *graph,
                     JsonNode *root,
                     GError **error)
{
    JsonObject *root_object = json_node_get_object (root);

    if (json_object_has_member (root_object, "prop-sets")) {
        JsonObject *sets = json_object_get_object_member (root_object, "prop-sets");
        json_object_foreach_member (sets, handle_json_prop_set, graph->priv);
    }

    if (json_object_has_member (root_object, "nodes")) {
        JsonArray *nodes = json_object_get_array_member (root_object, "nodes");
        GList *elements = json_array_get_elements (nodes);

        for (GList *it = g_list_first (elements); it != NULL; it = g_list_next (it)) {
            if (!handle_json_task_node (it->data, graph->priv, error)) {
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
            json_array_foreach_element (edges, handle_json_task_edge, graph);
        }
    }
}

static gboolean
handle_json_task_node (JsonNode *element,
                       UfoTaskGraphPrivate *priv,
                       GError **error)
{
    UfoTaskNode *plugin;
    JsonObject *object;
    GError *tmp_error = NULL;
    const gchar *name;
    const gchar *plugin_name;

    object = json_node_get_object (element);

    if (!json_object_has_member (object, "plugin") ||
        !json_object_has_member (object, "name")) {
        g_set_error (error, UFO_TASK_GRAPH_ERROR, UFO_TASK_GRAPH_ERROR_JSON_KEY,
                     "Node does not have `plugin' or `name' key");
        return FALSE;
    }

    plugin_name = json_object_get_string_member (object, "plugin");
    plugin = ufo_plugin_manager_get_task (priv->manager, plugin_name, &tmp_error);
    ufo_task_node_set_plugin_name (plugin, plugin_name);

    if (tmp_error != NULL) {
        g_propagate_error (error, tmp_error);
        return FALSE;
    }

    name = json_object_get_string_member (object, "name");

    if (g_hash_table_lookup (priv->json_nodes, name) != NULL)
        g_error ("Duplicate name `%s' found", name);

    g_hash_table_insert (priv->json_nodes, g_strdup (name), plugin);

    if (json_object_has_member (object, "properties")) {
        JsonObject *prop_object = json_object_get_object_member (object, "properties");
        json_object_foreach_member (prop_object, handle_json_single_prop, plugin);
    }

    if (json_object_has_member (object, "prop-refs")) {
        JsonArray *prop_refs;

        prop_refs = json_object_get_array_member (object, "prop-refs");

        for (guint i = 0; i < json_array_get_length (prop_refs); i++) {
            const gchar *ref_name = json_array_get_string_element (prop_refs, i);
            JsonObject *prop_set = g_hash_table_lookup (priv->prop_sets, ref_name);

            if (prop_set == NULL) {
                g_warning ("No property set `%s' found in `prop-sets'", ref_name);
            }
            else {
                json_object_foreach_member (prop_set,
                                            handle_json_single_prop,
                                            plugin);
            }
        }
    }

    return TRUE;
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
handle_json_single_prop (JsonObject *object,
                         const gchar *name,
                         JsonNode *node,
                         gpointer user)
{
    GValue val = {0,};
    if (!JSON_NODE_HOLDS_NULL (node)) {
        json_node_get_value (node, &val);
        g_object_set_property (G_OBJECT(user), name, &val);
    }
}

static void
add_task_node_to_json_array (UfoTaskNode *node, JsonArray *array)
{
    JsonObject *node_object;
    JsonNode *prop_node;

    node_object = json_object_new ();
    const gchar *plugin_name = ufo_task_node_get_plugin_name (node);
    g_assert (plugin_name != NULL);
    json_object_set_string_member (node_object, "plugin", plugin_name);
                                   
    const gchar *name = ufo_task_node_get_unique_name (node);
    g_assert (name != NULL);
    json_object_set_string_member (node_object, "name", name);

    prop_node = json_gobject_serialize (G_OBJECT (node));
    json_object_set_member (node_object, "properties", prop_node);
    json_array_add_object_element (array, node_object);
}

static JsonObject *
json_object_from_ufo_node (UfoNode *node)
{
    JsonObject *object;

    object = json_object_new ();
    const gchar *unique_name = ufo_task_node_get_unique_name (UFO_TASK_NODE (node));
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
