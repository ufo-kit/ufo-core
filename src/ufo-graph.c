/**
 * SECTION:ufo-graph
 * @Short_description: Organize filters
 * @Title: UfoGraph
 */

#include <glib.h>
#include <json-glib/json-glib.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "config.h"
#include "ufo-graph.h"
#include "ufo-aux.h"
#include "ufo-filter-source.h"
#include "ufo-plugin-manager.h"

G_DEFINE_TYPE (UfoGraph, ufo_graph, G_TYPE_OBJECT)

#define UFO_GRAPH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GRAPH, UfoGraphPrivate))

/**
 * UfoGraphError:
 * @UFO_GRAPH_ERROR_ALREADY_LOAD: Graph is already loaded
 * @UFO_GRAPH_ERROR_JSON_KEY: Specified key not found in JSON file
 *
 * Possible errors when loading the graph from JSON.
 */
GQuark
ufo_graph_error_quark(void)
{
    return g_quark_from_static_string("ufo-graph-error-quark");
}

typedef struct {
    UfoFilter   *from;
    guint        from_port;
    UfoFilter   *to;
    guint        to_port;
} Connection;

struct _UfoGraphPrivate {
    UfoPluginManager    *plugin_manager;
    GHashTable          *prop_sets;  /**< maps from gchar* to JsonNode */
    GList               *connections;
    GHashTable          *json_filters;
};

enum {
    PROP_0,
    N_PROPERTIES
};

/* static GParamSpec *graph_properties[N_PROPERTIES] = { NULL, }; */

static void
handle_json_prop_set (JsonObject    *object,
                      const gchar   *name,
                      JsonNode      *node,
                      gpointer       user)
{
    UfoGraphPrivate *priv;
    JsonObject *properties;

    priv = (UfoGraphPrivate *) user;
    properties = json_object_get_object_member (object, name);
    json_object_ref (properties);
    g_hash_table_insert (priv->prop_sets, g_strdup (name), properties);
}

static void
handle_json_single_prop (JsonObject  *object,
                         const gchar *name,
                         JsonNode    *node,
                         gpointer     user)
{
    GValue val = {0,};
    json_node_get_value (node, &val);
    g_object_set_property (G_OBJECT(user), name, &val);
}

static gboolean
handle_json_filter_node (JsonNode         *element,
                         UfoGraphPrivate  *priv,
                         GError          **error)
{
    UfoFilter *plugin;
    JsonObject *object;
    GError *tmp_error = NULL;
    const gchar *name;
    const gchar *plugin_name;

    object = json_node_get_object (element);

    if (!json_object_has_member (object, "plugin") ||
        !json_object_has_member (object, "name")) {
        g_set_error (error, UFO_GRAPH_ERROR, UFO_GRAPH_ERROR_JSON_KEY,
                     "Node does not have `plugin' or `name' key");
        return FALSE;
    }

    plugin_name = json_object_get_string_member (object, "plugin");
    plugin = ufo_plugin_manager_get_filter (priv->plugin_manager, plugin_name, &tmp_error);

    if (tmp_error != NULL) {
        g_propagate_error (error, tmp_error);
        return FALSE;
    }

    name = json_object_get_string_member (object, "name");

    if (g_hash_table_lookup (priv->json_filters, name) != NULL)
        g_error ("Duplicate name `%s' found", name);

    g_hash_table_insert (priv->json_filters, g_strdup (name), plugin);

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
handle_json_filter_edge (JsonArray *array,
                         guint      index,
                         JsonNode  *element,
                         gpointer   user)
{
    UfoGraph *graph = user;
    UfoGraphPrivate *priv = graph->priv;
    JsonObject *edge;
    UfoFilter  *from_filter, *to_filter;
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
    from_filter = g_hash_table_lookup (priv->json_filters, from_name);
    to_filter   = g_hash_table_lookup (priv->json_filters, to_name);

    ufo_graph_connect_filters_full (graph,
                                    from_filter, from_port,
                                    to_filter, to_port,
                                    &error);

    if (error != NULL)
        g_warning ("%s", error->message);
}

static void
graph_build (UfoGraph *graph, JsonNode *root, GError **error)
{
    JsonObject *root_object = json_node_get_object(root);

    if (json_object_has_member(root_object, "prop-sets")) {
        JsonObject *sets = json_object_get_object_member (root_object, "prop-sets");
        json_object_foreach_member (sets, handle_json_prop_set, graph->priv);
    }

    if (json_object_has_member (root_object, "nodes")) {
        JsonArray *nodes = json_object_get_array_member (root_object, "nodes");
        GList *elements = json_array_get_elements (nodes);

        for (GList *it = g_list_first (elements); it != NULL; it = g_list_next (it)) {
            if (!handle_json_filter_node (it->data, graph->priv, error)) {
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
            json_array_foreach_element (edges, handle_json_filter_edge, graph);
        }
    }
}

static JsonObject *
json_object_from_ufo_filter (UfoFilter *filter)
{
    JsonObject *object = json_object_new ();

    json_object_set_string_member (object, "name", ufo_filter_get_unique_name (filter));
    return object;
}

static void
graph_add_filter_to_json_array(gpointer data, gpointer user_data)
{
    g_return_if_fail (UFO_IS_FILTER (data));

    UfoFilter *filter = UFO_FILTER (data);
    JsonArray *array = (JsonArray *) user_data;
    JsonObject *node_object = json_object_new ();

    const gchar *plugin_name = ufo_filter_get_plugin_name (filter);
    json_object_set_string_member (node_object, "plugin", plugin_name);

    json_object_set_string_member (node_object, "name", ufo_filter_get_unique_name (filter));

    JsonNode *prop_node = json_gobject_serialize (G_OBJECT (filter));
    json_object_set_member (node_object, "properties", prop_node);

    json_array_add_object_element (array, node_object);
}

static void
add_nodes_from_connection (gpointer data, gpointer user)
{
    Connection *connection = (Connection *) data;
    GHashTable *filter_set = (GHashTable *) user;

    g_hash_table_insert (filter_set, connection->from, NULL);
    g_hash_table_insert (filter_set, connection->to, NULL);
}

static void
add_edge_from_connection (gpointer data, gpointer user)
{
    Connection  *connection = (Connection *) data;
    JsonArray   *edges      = (JsonArray *) user;
    JsonObject  *to_object  = json_object_from_ufo_filter (connection->to);
    JsonObject  *from_object = json_object_from_ufo_filter (connection->from);
    JsonObject  *edge_object = json_object_new ();

    json_object_set_int_member (to_object, "input", connection->to_port);
    json_object_set_int_member (from_object, "output", connection->from_port);
    json_object_set_object_member (edge_object, "to", to_object);
    json_object_set_object_member (edge_object, "from", from_object);
    json_array_add_object_element (edges, edge_object);
}

/**
 * ufo_graph_new:
 *
 * Create a new #UfoGraph.
 *
 * Return value: A #UfoGraph.
 */
UfoGraph *
ufo_graph_new (void)
{
    return UFO_GRAPH (g_object_new (UFO_TYPE_GRAPH, NULL));
}

/**
 * ufo_graph_read_from_json:
 * @graph: A #UfoGraph.
 * @manager: A #UfoPluginManager used to load the filters
 * @filename: Path and filename to the JSON file
 * @error: Indicates error in case of failed file loading or parsing
 *
 * Read a JSON configuration file to fill the filter structure of @graph.
 */
void
ufo_graph_read_from_json (UfoGraph *graph, UfoPluginManager *manager, const gchar *filename, GError **error)
{
    g_return_if_fail (UFO_IS_GRAPH(graph) && UFO_IS_PLUGIN_MANAGER (manager) && (filename != NULL));
    JsonParser *json_parser = json_parser_new ();
    GError *tmp_error = NULL;

    if (!json_parser_load_from_file (json_parser, filename, &tmp_error)) {
        g_propagate_prefixed_error (error, tmp_error, "Parsing JSON: ");
        return;
    }

    graph->priv->plugin_manager = manager;
    g_object_ref (manager);

    graph_build (graph, json_parser_get_root (json_parser), error);
    g_object_unref (json_parser);
}

/**
 * ufo_graph_save_to_json:
 * @graph: A #UfoGraph.
 * @filename: Path and filename to the JSON file
 * @error: Indicates error in case of failed file saving
 *
 * Save a JSON configuration file with the filter structure of @graph.
 */
void
ufo_graph_save_to_json (UfoGraph *graph, const gchar *filename, GError **error)
{
    g_return_if_fail (UFO_IS_GRAPH (graph) && (filename != NULL));

    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE (graph);
    GError *tmp_error = NULL;

    JsonGenerator *json_generator = json_generator_new ();
    JsonNode *root_node = json_node_new (JSON_NODE_OBJECT);
    JsonObject *root_object = json_object_new ();
    JsonArray *nodes = json_array_new ();
    JsonArray *edges = json_array_new ();

    GHashTable *filter_set = g_hash_table_new (g_direct_hash, g_direct_equal);

    g_list_foreach (priv->connections, add_nodes_from_connection, filter_set);

    GList *filters = g_hash_table_get_keys (filter_set);
    g_list_foreach (filters, graph_add_filter_to_json_array, nodes);
    g_list_free (filters);
    g_hash_table_destroy (filter_set);

    g_list_foreach (priv->connections, add_edge_from_connection, edges);

    json_object_set_array_member(root_object, "nodes", nodes);
    json_object_set_array_member(root_object, "edges", edges);
    json_node_set_object(root_node, root_object);
    json_generator_set_root(json_generator, root_node);

    if (!json_generator_to_file(json_generator, filename, &tmp_error)) {
        g_propagate_error(error, tmp_error);
        return;
    }

    g_object_unref(json_generator);
}

/**
 * ufo_graph_connect_filters:
 * @graph: A #UfoGraph
 * @from: Source filter
 * @to: Destination filter
 * @error: return location for error
 *
 * Connect to filters using their default input and output ports.
 */
void
ufo_graph_connect_filters (UfoGraph *graph, UfoFilter *from, UfoFilter *to, GError **error)
{
    ufo_graph_connect_filters_full (graph, from, 0, to, 0, error);
}

/**
 * ufo_graph_connect_filters_full:
 * @graph: A #UfoGraph
 * @from: Source filter
 * @from_port: Source output port
 * @to: Destination filter
 * @to_port: Destination input port
 * @error: return location for error
 *
 * Connect two filters with the specified input and output ports.
 */
void
ufo_graph_connect_filters_full (UfoGraph    *graph,
                                UfoFilter   *from,
                                guint        from_port,
                                UfoFilter   *to,
                                guint        to_port,
                                GError     **error)
{
    UfoGraphPrivate *priv;
    UfoChannel      *channel;
    Connection      *connection;

    g_return_if_fail (UFO_IS_GRAPH (graph) && UFO_IS_FILTER (from) && UFO_IS_FILTER (to));
    priv = graph->priv;

    for (GList *it = g_list_first (priv->connections); it != NULL; it = g_list_next (it)) {
        connection = (Connection *) it->data;

        if (connection->from == from &&
            connection->to == to &&
            connection->from_port == from_port &&
            connection->to_port == to_port) {

            g_warning ("Primary connection between %s and %s exists already",
                       ufo_filter_get_unique_name (from),
                       ufo_filter_get_unique_name (to));
            return;
        }
    }

    connection = g_new0 (Connection, 1);

    connection->from = from;
    connection->from_port = from_port;
    connection->to = to;
    connection->to_port= to_port;
    priv->connections = g_list_append (priv->connections, connection);

    channel = ufo_filter_get_output_channel (from, from_port);

    if (channel == NULL) {
        channel = ufo_filter_get_input_channel (to, to_port);

        if (channel == NULL)
            channel = ufo_channel_new ();
    }

    ufo_filter_set_output_channel (from, from_port, channel);
    ufo_filter_set_input_channel (to, to_port, channel);
    ufo_channel_ref (channel);
}

/**
 * ufo_graph_get_filters:
 * @graph: A #UfoGraph
 *
 * Return a list of all filter nodes of @graph.
 *
 * Returns: (element-type UfoFilter) (transfer full): List of filter nodes. Use g_list_free()
 *      when done using the list.
 */
GList *
ufo_graph_get_filters (UfoGraph *graph)
{
    UfoGraphPrivate *priv;
    Connection      *connection;
    GHashTable      *filters;
    GList           *result;

    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    priv = graph->priv;

    filters = g_hash_table_new (g_direct_hash, g_direct_equal);

    for (GList *it = g_list_first (priv->connections); it != NULL; it = g_list_next (it)) {
        connection = (Connection *) it->data;
        g_hash_table_insert (filters, connection->from, NULL);
        g_hash_table_insert (filters, connection->to, NULL);
    }

    result = g_hash_table_get_keys (filters);
    g_hash_table_destroy (filters);
    return result;
}

/**
 * ufo_graph_get_roots:
 * @graph: A #UfoGraph
 *
 * Return a list of #UfoFilterSource nodes in @graph that do not have any
 * parents.
 *
 * Returns: (element-type UfoFilter) (transfer none): List of filter nodes. Use
 * g_list_free() when done using the list.
 */
GList *
ufo_graph_get_roots (UfoGraph *graph)
{
    UfoGraphPrivate *priv;
    Connection      *connection;
    GHashTable      *filters;
    GList           *result = NULL;

    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    priv = graph->priv;
    filters = g_hash_table_new (g_direct_hash, g_direct_equal);

    for (GList *it = g_list_first (priv->connections); it != NULL; it = g_list_next (it)) {
        connection = (Connection *) it->data;

        if (UFO_IS_FILTER_SOURCE (connection->from))
            g_hash_table_insert (filters, connection->from, NULL);
    }

    result = g_hash_table_get_keys (filters);
    g_hash_table_destroy (filters);
    return result;
}

/**
 * ufo_graph_get_parents:
 * @graph: A #UfoGraph
 * @filter: A #UfoFilter
 *
 * Return a list of nodes in @graph that connect to @filter.
 *
 * Returns: (element-type UfoFilter) (transfer none): List of filter nodes. Use g_list_free()
 *      when done using the list.
 */
GList *
ufo_graph_get_parents (UfoGraph   *graph,
                       UfoFilter  *filter)
{
    UfoGraphPrivate *priv;
    Connection      *connection;
    GList           *result = NULL;

    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    priv = graph->priv;

    for (GList *it = g_list_first (priv->connections); it != NULL; it = g_list_next (it)) {
        connection = (Connection *) it->data;

        if (connection->to == filter)
            result = g_list_append (result, connection->from);
    }

    return result;
}

/**
 * ufo_graph_get_children:
 * @graph: A #UfoGraph
 * @filter: A #UfoFilter
 *
 * Return a list of nodes in @graph that @filter connects to.
 *
 * Returns: (element-type UfoFilter) (transfer none): List of filter nodes. Use g_list_free()
 *      when done using the list.
 */
GList *
ufo_graph_get_children (UfoGraph   *graph,
                        UfoFilter  *filter)
{
    UfoGraphPrivate *priv;
    Connection      *connection;
    GList           *result = NULL;

    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    priv = graph->priv;

    for (GList *it = g_list_first (priv->connections); it != NULL; it = g_list_next (it)) {
        connection = (Connection *) it->data;

        if (connection->from == filter)
            result = g_list_append (result, connection->to);
    }

    return result;
}

/**
 * ufo_graph_get_siblings:
 * @graph: A #UfoGraph
 * @filter: A #UfoFilter
 *
 * Return a list of nodes in @graph that share the same parent node with
 * @filter.
 *
 * Returns: (element-type UfoFilter) (transfer none): List of filter nodes. Use g_list_free()
 *      when done using the list.
 */
GList *
ufo_graph_get_siblings (UfoGraph    *graph,
                        UfoFilter   *filter)
{
    UfoGraphPrivate *priv;
    Connection      *connection;
    GList           *parents;
    GList           *result = NULL;

    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    priv = graph->priv;
    parents = ufo_graph_get_parents (graph, filter);

    for (GList *it = g_list_first (priv->connections); it != NULL; it = g_list_next (it)) {
        connection = (Connection *) it->data;

        for (GList *jt = g_list_first (parents); jt != NULL; jt = g_list_next (jt)) {
            UfoFilter *predecessor = (UfoFilter *) jt->data;

            if (connection->to != filter && connection->from == predecessor)
                result = g_list_append (result, connection->to);
        }
    }

    g_list_free (parents);
    return result;
}

static void
unref_list_elements (GList *l)
{
    g_list_foreach (l, (GFunc) g_object_unref, NULL);
}

static void
ufo_graph_dispose(GObject *object)
{
    UfoGraph *graph = UFO_GRAPH (object);
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE (graph);

    unref_list_elements (g_hash_table_get_values (priv->json_filters));

    ufo_unref_stored_object ((GObject **) &priv->plugin_manager);

    G_OBJECT_CLASS (ufo_graph_parent_class)->dispose (object);
    g_message ("UfoGraph: disposed");
}

static void
ufo_graph_finalize (GObject *object)
{
    UfoGraph *self = UFO_GRAPH (object);
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE (self);

    g_hash_table_destroy (priv->prop_sets);
    g_hash_table_destroy (priv->json_filters);

    g_list_foreach (priv->connections, (GFunc) g_free, NULL);
    g_list_free (priv->connections);

    priv->connections   = NULL;
    priv->prop_sets = NULL;

    G_OBJECT_CLASS (ufo_graph_parent_class)->finalize (object);
    g_message ("UfoGraph: finalized");
}

static void
ufo_graph_constructed (GObject *object)
{
    if (G_OBJECT_CLASS (ufo_graph_parent_class)->constructed != NULL)
        G_OBJECT_CLASS (ufo_graph_parent_class)->constructed (object);
}

static void
ufo_graph_set_property(GObject      *object,
                       guint         property_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_graph_get_property(GObject      *object,
                       guint         property_id,
                       GValue       *value,
                       GParamSpec   *pspec)
{
    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_graph_class_init (UfoGraphClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->set_property = ufo_graph_set_property;
    gobject_class->get_property = ufo_graph_get_property;
    gobject_class->dispose      = ufo_graph_dispose;
    gobject_class->finalize     = ufo_graph_finalize;
    gobject_class->constructed  = ufo_graph_constructed;

    g_type_class_add_private(klass, sizeof(UfoGraphPrivate));
}

static void
ufo_graph_init (UfoGraph *self)
{
    UfoGraphPrivate *priv;
    self->priv = priv = UFO_GRAPH_GET_PRIVATE (self);
    priv->connections = NULL;
    priv->prop_sets = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) json_object_unref);
    priv->json_filters = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    g_thread_init (NULL);
}

