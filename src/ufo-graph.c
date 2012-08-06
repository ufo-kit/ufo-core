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
#include "ufo-resource-manager.h"
#include "ufo-plugin-manager.h"
#include "ufo-base-scheduler.h"

G_DEFINE_TYPE(UfoGraph, ufo_graph, G_TYPE_OBJECT)

#define UFO_GRAPH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GRAPH, UfoGraphPrivate))

/**
 * UfoGraphError:
 * @UFO_GRAPH_ERROR_ALREADY_LOAD: Graph is already loaded
 *
 * Possible errors when loading the graph from JSON.
 */
GQuark ufo_graph_error_quark(void)
{
    return g_quark_from_static_string("ufo-graph-error-quark");
}

struct _UfoGraphPrivate {
    UfoPluginManager    *plugin_manager;
    UfoResourceManager  *manager;
    GHashTable          *property_sets;  /**< maps from gchar* to JsonNode */
    gchar               *paths;
    GList               *relations;
    GHashTable          *json_filters;
};

enum {
    PROP_0,
    PROP_PATHS,
    PROP_RESOURCE_MANAGER,
    N_PROPERTIES
};

static GParamSpec *graph_properties[N_PROPERTIES] = { NULL, };

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

static void
handle_json_filter_node (JsonArray *array,
                         guint      index,
                         JsonNode  *element,
                         gpointer   user)
{
    UfoGraph *graph = user;
    UfoGraphPrivate *priv = graph->priv;
    UfoFilter *plugin;
    JsonObject *object;
    GError *error = NULL;
    const gchar *name;
    const gchar *plugin_name;

    object = json_node_get_object (element);

    if (!json_object_has_member (object, "plugin") ||
        !json_object_has_member (object, "name")) {
        g_error ("Node does not have `plugin' or `name' key");
        return;
    }

    plugin_name = json_object_get_string_member (object, "plugin");
    plugin = ufo_plugin_manager_get_filter (priv->plugin_manager, plugin_name, &error);

    ufo_filter_set_resource_manager (plugin, priv->manager);

    name = json_object_get_string_member (object, "name");
    g_hash_table_insert (priv->json_filters, g_strdup (name), plugin);

    if (json_object_has_member (object, "properties")) {
        JsonObject *prop_object = json_object_get_object_member (object, "properties");
        json_object_foreach_member (prop_object, handle_json_single_prop, plugin);
    }
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
graph_build (UfoGraph *self, JsonNode *root)
{
    JsonObject *root_object = json_node_get_object(root);

    /* if (json_object_has_member(root_object, "prop-sets")) { */
    /*     JsonObject *sets = json_object_get_object_member(root_object, "prop-sets"); */
    /*     json_object_foreach_member(sets, graph_handle_json_propset, self); */
    /* } */

    if (json_object_has_member (root_object, "nodes")) {
        JsonArray *nodes = json_object_get_array_member (root_object, "nodes");
        json_array_foreach_element (nodes, handle_json_filter_node, self);

        /* We only check edges if we have nodes, anything else doesn't make much
         * sense. */
        if (json_object_has_member (root_object, "edges")) {
            JsonArray *edges = json_object_get_array_member (root_object, "edges");
            json_array_foreach_element (edges, handle_json_filter_edge, self);
        }
    }
}

static JsonObject *
json_object_from_ufo_filter (UfoFilter *filter)
{
    JsonObject *object = json_object_new ();
    const gchar *plugin_name = ufo_filter_get_plugin_name (filter);
    gchar *unique_name = g_strdup_printf ("%s-%p", plugin_name, (gpointer) filter);

    json_object_set_string_member (object, "name", unique_name);
    g_free (unique_name);
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

    gchar *unique_name = g_strdup_printf ("%s-%p", plugin_name, (gpointer) filter);
    json_object_set_string_member (node_object, "name", unique_name);

    JsonNode *prop_node = json_gobject_serialize (G_OBJECT (filter));
    json_object_set_member (node_object, "properties", prop_node);

    json_array_add_object_element (array, node_object);

    g_free (unique_name);
}

static void
add_filters_from_relation (gpointer data, gpointer user)
{
    UfoRelation *relation = UFO_RELATION (data);
    GHashTable *filter_set = (GHashTable *) user;
    UfoFilter *producer = ufo_relation_get_producer (relation);
    GList *consumers = ufo_relation_get_consumers (relation);

    g_hash_table_insert (filter_set, producer, NULL);

    for (GList *it = g_list_first (consumers); it != NULL; it = g_list_next (it))
        g_hash_table_insert (filter_set, it->data, NULL);
}

static void
add_edges_from_relation (gpointer data, gpointer user)
{
    UfoRelation *relation = UFO_RELATION (data);
    JsonArray *edges = (JsonArray *) user;
    UfoFilter *producer = ufo_relation_get_producer (relation);

    JsonObject *relation_object = json_object_from_ufo_filter (producer);

    JsonArray *consumer_objects = json_array_new ();
    GList *consumers = ufo_relation_get_consumers (relation);

    for (GList *it = g_list_first (consumers); it != NULL; it = g_list_next (it)) {
        UfoFilter *consumer = UFO_FILTER (it->data);
        JsonObject *consumer_object = json_object_from_ufo_filter (consumer);

        json_object_set_int_member (consumer_object, "input",
                                    ufo_relation_get_consumer_port (relation, consumer));
        json_array_add_object_element (consumer_objects, consumer_object);
    }


    json_object_set_int_member (relation_object, "output", ufo_relation_get_producer_port (relation));
    json_object_set_array_member (relation_object, "consumers", consumer_objects);
    json_array_add_object_element (edges, relation_object);
}

/**
 * ufo_graph_new:
 * @paths: A string with a colon-separated list of paths that are used to search
 *      for OpenCL kernel files and header files included by OpenCL kernels.
 *
 * Create a new #UfoGraph.
 *
 * Return value: A #UfoGraph.
 */
UfoGraph *
ufo_graph_new (const gchar *paths)
{
    return UFO_GRAPH (g_object_new (UFO_TYPE_GRAPH, "paths", paths, NULL));
}

/**
 * ufo_graph_read_from_json:
 * @graph: A #UfoGraph.
 * @filename: Path and filename to the JSON file
 * @error: Indicates error in case of failed file loading or parsing
 *
 * Read a JSON configuration file to fill the filter structure of @graph.
 */
void ufo_graph_read_from_json(UfoGraph *graph, UfoPluginManager *manager, const gchar *filename, GError **error)
{
    g_return_if_fail(UFO_IS_GRAPH(graph) || (filename != NULL));
    JsonParser *json_parser = json_parser_new();
    GError *tmp_error = NULL;

    if (!json_parser_load_from_file(json_parser, filename, &tmp_error)) {
        g_propagate_error(error, tmp_error);
        return;
    }

    graph->priv->plugin_manager = manager;
    graph_build(graph, json_parser_get_root(json_parser));
    g_object_unref(json_parser);
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
ufo_graph_save_to_json(UfoGraph *graph, const gchar *filename, GError **error)
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
    g_list_foreach (priv->relations, add_filters_from_relation, filter_set);

    GList *filters = g_hash_table_get_keys (filter_set);
    g_list_foreach (filters, graph_add_filter_to_json_array, nodes);
    g_list_free (filters);
    g_hash_table_destroy (filter_set);

    g_list_foreach (priv->relations, add_edges_from_relation, edges);

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
 * ufo_graph_run:
 * @graph: A #UfoGraph.
 * @error: Return location for error
 *
 * Start execution of all UfoElements in the UfoGraph until no more data is
 * produced
 */
void ufo_graph_run(UfoGraph *graph, GError **error)
{
    UfoGraphPrivate  *priv;
    UfoBaseScheduler *scheduler;

    g_return_if_fail (UFO_IS_GRAPH(graph));

    priv = UFO_GRAPH_GET_PRIVATE (graph);
    scheduler = ufo_base_scheduler_new (priv->manager);
    ufo_base_scheduler_run (scheduler, priv->relations, error);
    g_object_unref (scheduler);
}


/**
 * ufo_graph_add_relation:
 * @graph: A #UfoGraph
 * @relation: A multi-edge that should is part of the graph object
 *
 * Add a new relation to the graph.
 */
void
ufo_graph_add_relation(UfoGraph *graph, UfoRelation *relation)
{
    g_return_if_fail(UFO_IS_GRAPH(graph) && UFO_IS_RELATION(relation));
    g_object_ref (relation);
    graph->priv->relations = g_list_append(graph->priv->relations, relation);
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
 * Connect to filters.
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
    UfoRelation     *relation;
    GError          *tmp_error = NULL;

    g_return_if_fail (UFO_IS_GRAPH (graph) && UFO_IS_FILTER (from) && UFO_IS_FILTER (to));
    priv = UFO_GRAPH_GET_PRIVATE (graph);

    /*
     * Check that we do not make the connection twice.
     */
    for (GList *it = g_list_first (priv->relations); it != NULL; it = g_list_next (it)) {
        relation = UFO_RELATION (it->data);

        if (ufo_relation_get_producer (relation) == from) {
            GList *consumers = ufo_relation_get_consumers (relation);
            if ((g_list_first (consumers))->data == to)
                g_warning ("Primary connection between %s-%p and %s-%p exists already",
                        ufo_filter_get_plugin_name (from), (gpointer) from,
                        ufo_filter_get_plugin_name (to), (gpointer) to);
        }
    }

    relation = ufo_relation_new (from, from_port, UFO_RELATION_MODE_DISTRIBUTE);
    ufo_relation_add_consumer (relation, to, to_port, &tmp_error);

    if (tmp_error == NULL) {
        /*
         * We don't call ufo_graph_add_relation() because we don't want to
         * reference the relation object once again.
         */
        priv->relations = g_list_append (priv->relations, relation);
    }
    else
        g_propagate_error (error, tmp_error);

    ufo_filter_set_resource_manager (from, priv->manager);
    ufo_filter_set_resource_manager (to, priv->manager);
}

/**
 * ufo_graph_get_resource_manager:
 * @graph: A #UfoGraph
 *
 * Returns the #UfoResourceManager associated with this graph.
 *
 * Returns: (transfer full): A #UfoResourceManager
 */
UfoResourceManager *
ufo_graph_get_resource_manager (UfoGraph *graph)
{
    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    return graph->priv->manager;
}

static void
ufo_graph_dispose(GObject *object)
{
    UfoGraph *graph = UFO_GRAPH (object);
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE (graph);

    g_list_foreach (priv->relations, (GFunc) g_object_unref, NULL);

    if (priv->plugin_manager != NULL) {
        g_object_unref (priv->plugin_manager);
        priv->plugin_manager = NULL;
    }

    g_object_unref (priv->manager);

    priv->manager = NULL;
    G_OBJECT_CLASS (ufo_graph_parent_class)->dispose (object);
    g_message ("UfoGraph: disposed");
}

static void
ufo_graph_finalize (GObject *object)
{
    UfoGraph *self = UFO_GRAPH (object);
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE (self);

    g_hash_table_destroy (priv->property_sets);
    g_list_free (priv->relations);
    g_free (priv->paths);

    priv->property_sets = NULL;
    priv->paths = NULL;
    G_OBJECT_CLASS (ufo_graph_parent_class)->finalize (object);
    g_message ("UfoGraph: finalized");
}

static void
ufo_graph_constructed (GObject *object)
{
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE (object);

    gchar *paths = g_strdup_printf ("%s:%s", priv->paths, LIB_FILTER_DIR);

    /* Create a new resource manager if none was passed during construction */
    if (priv->manager == NULL) {
        priv->manager = ufo_resource_manager_new ();
        g_message ("UfoGraph: Created new resource manager %p", (gpointer) priv->manager);
    }
    else
        g_message ("UfoGraph: Use provided resource manager %p", (gpointer) priv->manager);

    g_object_ref (priv->manager);

    /* Append LIB_FILTER_DIR to the search path */
    g_free (priv->paths);
    priv->paths = paths;

    ufo_resource_manager_add_paths (priv->manager, priv->paths);

    if (G_OBJECT_CLASS (ufo_graph_parent_class)->constructed != NULL)
        G_OBJECT_CLASS (ufo_graph_parent_class)->constructed (object);
}

static void
ufo_graph_set_property(GObject      *object,
                       guint         property_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_PATHS:
            g_free(priv->paths);
            priv->paths = g_strdup(g_value_get_string(value));
            break;

        case PROP_RESOURCE_MANAGER:
            priv->manager = g_value_get_object (value);
            break;

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
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_PATHS:
            g_value_set_string (value, priv->paths);
            break;

        case PROP_RESOURCE_MANAGER:
            g_value_set_object (value, priv->manager);
            break;

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
    gobject_class->dispose = ufo_graph_dispose;
    gobject_class->finalize = ufo_graph_finalize;
    gobject_class->constructed = ufo_graph_constructed;

    /**
     * UfoGraph:paths:
     *
     * List of colon-separated paths pointing to possible filter and kernel file
     * locations.
     */
    graph_properties[PROP_PATHS] =
        g_param_spec_string ("paths",
                             "List of :-separated paths pointing to possible filter locations",
                             "List of :-separated paths pointing to possible filter locations",
                             ".",
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    graph_properties[PROP_RESOURCE_MANAGER] =
        g_param_spec_object ("resource-manager",
                             "A UfoResourceManager",
                             "The UfoResourceManager that is used to access resources",
                             UFO_TYPE_RESOURCE_MANAGER,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    g_object_class_install_property (gobject_class, PROP_PATHS, graph_properties[PROP_PATHS]);
    g_object_class_install_property (gobject_class, PROP_RESOURCE_MANAGER, graph_properties[PROP_RESOURCE_MANAGER]);

    g_type_class_add_private(klass, sizeof(UfoGraphPrivate));
}

static void
ufo_graph_init (UfoGraph *self)
{
    UfoGraphPrivate *priv;
    self->priv = priv = UFO_GRAPH_GET_PRIVATE (self);
    priv->property_sets = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    priv->paths = NULL;
    priv->relations = NULL;
    priv->manager = NULL;
    priv->json_filters = g_hash_table_new (g_str_hash, g_str_equal);
}

