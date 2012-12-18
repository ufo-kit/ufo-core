/**
 * SECTION:ufo-task-graph
 * @Short_description: Hold and manage #UfoTaskNode elements.
 * @Title: UfoTaskGraph
 */

#include <ufo-task-graph.h>
#include <ufo-task-node.h>
#include <ufo-cpu-task-iface.h>
#include <ufo-gpu-task-iface.h>
#include <ufo-remote-task.h>
#include <ufo-input-task.h>
#include <json-glib/json-glib.h>

G_DEFINE_TYPE (UfoTaskGraph, ufo_task_graph, UFO_TYPE_GRAPH)

#define UFO_TASK_GRAPH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_TASK_GRAPH, UfoTaskGraphPrivate))

struct _UfoTaskGraphPrivate {
    UfoPluginManager *manager;
    GHashTable *prop_sets;
    GHashTable *json_nodes;
};

static void add_nodes_from_json     (UfoTaskGraph *, JsonNode *, GError **);
static void handle_json_prop_set    (JsonObject *, const gchar *, JsonNode *, gpointer user);
static void handle_json_single_prop (JsonObject *, const gchar *, JsonNode *, gpointer user);
static void handle_json_task_edge   (JsonArray *, guint, JsonNode *, gpointer);
static gboolean handle_json_task_node (JsonNode *, UfoTaskGraphPrivate *priv, GError **error);
static void add_task_node_to_json_array (UfoNode *, JsonArray *);
static JsonObject *json_object_from_ufo_node (UfoNode *node);

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

/**
 * ufo_task_graph_read_from_json:
 * @graph: A #UfoTaskGraph.
 * @manager: A #UfoPluginManager used to load the filters
 * @filename: Path and filename to the JSON file
 * @error: Indicates error in case of failed file loading or parsing
 *
 * Read a JSON configuration file to fill the filter structure of @graph.
 */
void
ufo_task_graph_read_from_json (UfoTaskGraph *graph,
                               UfoPluginManager *manager,
                               const gchar *filename,
                               GError **error)
{
    UfoTaskGraphPrivate *priv;
    JsonParser *json_parser;
    GError *tmp_error = NULL;

    g_return_if_fail (UFO_IS_TASK_GRAPH (graph) && UFO_IS_PLUGIN_MANAGER (manager) && (filename != NULL));

    priv = graph->priv;
    json_parser = json_parser_new ();

    if (!json_parser_load_from_file (json_parser, filename, &tmp_error)) {
        g_propagate_prefixed_error (error, tmp_error, "Parsing JSON: ");
        return;
    }

    priv->manager = manager;
    g_object_ref (manager);

    add_nodes_from_json (graph, json_parser_get_root (json_parser), error);
    g_object_unref (json_parser);
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
    UfoTaskGraphPrivate *priv;
    GList *task_nodes;
    GError *tmp_error = NULL;

    g_return_if_fail (UFO_IS_GRAPH (graph) && (filename != NULL));

    priv = graph->priv;

    JsonGenerator *json_generator = json_generator_new ();
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

    json_object_set_array_member (root_object, "nodes", nodes);
    json_object_set_array_member (root_object, "edges", edges);
    json_node_set_object (root_node, root_object);
    json_generator_set_root (json_generator, root_node);

    if (!json_generator_to_file (json_generator, filename, &tmp_error)) {
        g_propagate_error (error, tmp_error);
        return;
    }

    json_node_free (root_node);
    g_object_unref (json_generator);
}

static gboolean
is_gpu_task (UfoNode *node)
{
    return UFO_IS_GPU_TASK (node);
}

/**
 * ufo_task_graph_split:
 * @task_graph: A #UfoTaskGraph
 * @arch_graph: A #UfoArchGraph
 *
 * Splits @task_graph in a way that most of the resources in @arch_graph can be
 * occupied. In the simple pipeline case, the longest possible GPU paths are
 * duplicated as much as there are GPUs in @arch_graph.
 */
void
ufo_task_graph_split (UfoTaskGraph *task_graph,
                      UfoArchGraph *arch_graph)
{
    GList *paths;
    guint n_gpus;

    paths = ufo_graph_get_paths (UFO_GRAPH (task_graph), is_gpu_task);
    n_gpus = ufo_arch_graph_get_num_gpus (arch_graph);

    for (GList *it = g_list_first (paths); it != NULL; it = g_list_next (it)) {
        GList *path = (GList *) it->data;

        for (guint i = 1; i < n_gpus; i++)
            ufo_graph_split (UFO_GRAPH (task_graph), path);

        g_list_free (path);
    }

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
               GList *gpu_nodes,
               GList *remote_nodes)
{
    GList *successors;
    guint index;
    guint n_gpus;

    n_gpus = g_list_length (gpu_nodes);
    successors = ufo_graph_get_successors (graph, node);
    index = 0;

    if (UFO_IS_GPU_TASK (node)) {
        ufo_task_node_set_proc_node (UFO_TASK_NODE (node),
                                     g_list_nth_data (gpu_nodes, proc_index));
    }

    if (UFO_IS_INPUT_TASK (node)) {
        ufo_task_node_set_proc_node (UFO_TASK_NODE (node),
                                     g_list_nth_data (gpu_nodes, proc_index));
    }

    if (UFO_IS_REMOTE_TASK (node)) {
        ufo_task_node_set_proc_node (UFO_TASK_NODE (node),
                                     g_list_nth_data (remote_nodes, 0));
    }

    for (GList *it = g_list_first (successors); it != NULL; it = g_list_next (it)) {
        map_proc_node (graph, UFO_NODE (it->data), proc_index + index, gpu_nodes, remote_nodes);
        index = (index + 1) % n_gpus;
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
    GList *remote_nodes;
    GList *roots;

    gpu_nodes = ufo_arch_graph_get_gpu_nodes (arch_graph);
    remote_nodes = ufo_arch_graph_get_remote_nodes (arch_graph);
    roots = ufo_graph_get_roots (UFO_GRAPH (task_graph));

    for (GList *it = g_list_first (roots); it != NULL; it = g_list_next (it))
        map_proc_node (UFO_GRAPH (task_graph), UFO_NODE (it->data), 0, gpu_nodes, remote_nodes);

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
    UfoNode *plugin;
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
    guint from_port, to_port;
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
    from_port = 0;

    if (json_object_has_member (from_object, "output"))
        from_port = (guint) json_object_get_int_member (from_object, "output");

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
    json_node_get_value (node, &val);
    g_object_set_property (G_OBJECT(user), name, &val);
}

static void
add_task_node_to_json_array (UfoNode *node, JsonArray *array)
{
    JsonObject *node_object;
    JsonNode *prop_node;

    node_object = json_object_new ();

    json_object_set_string_member (node_object,
                                   "plugin",
                                   ufo_task_node_get_plugin_name (UFO_TASK_NODE (node)));

    json_object_set_string_member (node_object,
                                   "name",
                                   ufo_task_node_get_unique_name (UFO_TASK_NODE (node)));

    prop_node = json_gobject_serialize (G_OBJECT (node));
    json_object_set_member (node_object, "properties", prop_node);
    json_array_add_object_element (array, node_object);
}

static JsonObject *
json_object_from_ufo_node (UfoNode *node)
{
    JsonObject *object;

    object = json_object_new ();
    json_object_set_string_member (object,
                                   "name",
                                   ufo_task_node_get_unique_name (UFO_TASK_NODE (node)));
    return object;
}

static void
ufo_task_graph_class_init (UfoTaskGraphClass *klass)
{
    g_type_class_add_private(klass, sizeof(UfoTaskGraphPrivate));
}

static void
ufo_task_graph_init (UfoTaskGraph *self)
{
    UfoTaskGraphPrivate *priv;
    self->priv = priv = UFO_TASK_GRAPH_GET_PRIVATE (self);

    priv->json_nodes = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              g_free, NULL);

    priv->prop_sets = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             g_free, (GDestroyNotify) json_object_unref);

    /* Maybe we should define a specific task node type from which all tasks
     * must inherit */
    ufo_graph_register_node_type (UFO_GRAPH (self), UFO_TYPE_NODE);
}
