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

G_DEFINE_TYPE(UfoGraph, ufo_graph, G_TYPE_OBJECT)

#define UFO_GRAPH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GRAPH, UfoGraphPrivate))

/**
 * UfoGraphError:
 * @UFO_GRAPH_ERROR_ALREADY_LOAD: Graph is already loaded
 */
GQuark ufo_graph_error_quark(void)
{
    return g_quark_from_static_string("ufo-graph-error-quark");
}

struct _UfoGraphPrivate {
    UfoPluginManager    *plugin_manager;
    UfoResourceManager  *resource_manager;
    GHashTable          *property_sets;  /**< maps from gchar* to JsonNode */
    gchar               *paths;
    GHashTable          *nodes;          /**< maps from gchar* to UfoFilter */
};

enum {
    PROP_0,
    PROP_PATHS,
    N_PROPERTIES
};

static GParamSpec *graph_properties[N_PROPERTIES] = { NULL, };

static void graph_handle_json_single_prop(JsonObject *object, const gchar *name, JsonNode *node, gpointer user)
{
    GValue val = {0,};
    json_node_get_value(node, &val);
    g_object_set_property(G_OBJECT(user), name, &val);
    g_value_unset(&val);
}

static void graph_handle_json_filter_node(JsonArray *array, guint index, JsonNode *node, gpointer user_data)
{
    g_return_if_fail(UFO_IS_GRAPH(user_data));

    UfoGraph *self = UFO_GRAPH(user_data);
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(self);

    JsonObject *object = json_node_get_object(node);
    /* TODO: check existence */
    const gchar *plugin_name = json_object_get_string_member(object, "plugin");
    const gchar *node_name = json_object_get_string_member(object, "name");
    UfoFilter *filter = ufo_graph_get_filter(self, plugin_name, NULL);

    /* TODO: check that the node name is unique */
    g_hash_table_insert(priv->nodes, g_strdup(node_name), filter);

    if (filter == NULL) {
        g_warning("Couldn't find plugin '%s'", plugin_name);
        return;
    }

    /* We can define "properties" for each filter ... */
    if (json_object_has_member(object, "properties")) {
        JsonObject *prop_object = json_object_get_object_member(object, "properties");
        json_object_foreach_member(prop_object,
                                   graph_handle_json_single_prop,
                                   filter);
    }

    /* ... and also add more through prop-refs */
    if (json_object_has_member(object, "prop-refs")) {
        JsonArray *prop_refs = json_object_get_array_member(object, "prop-refs");

        for (guint i = 0; i < json_array_get_length(prop_refs); i++) {
            const gchar *set_name = json_array_get_string_element(prop_refs, i);
            JsonObject *prop_set = g_hash_table_lookup(priv->property_sets, set_name);

            if (prop_set == NULL)
                g_warning("No property set '%s' in 'prop-sets'", set_name);
            else {
                json_object_foreach_member(prop_set,
                        graph_handle_json_single_prop,
                        filter);
            }
        }

        json_array_unref(prop_refs);
    }
}

static void graph_handle_json_filter_edge(JsonArray *array, guint index, JsonNode *node, gpointer user_data)
{
    g_return_if_fail(UFO_IS_GRAPH(user_data));
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(user_data);
    JsonObject *object = json_node_get_object(node);
    const gchar *from_name = json_object_get_string_member(object, "from");
    const gchar *to_name = json_object_get_string_member(object, "to");
    UfoFilter *from = g_hash_table_lookup(priv->nodes, from_name);
    UfoFilter *to = g_hash_table_lookup(priv->nodes, to_name);

    ufo_filter_connect_to(from, to, NULL);
}

static void graph_handle_json_propset(JsonObject *object,
        const gchar *member_name,
        JsonNode *member_node,
        gpointer user_data)
{
    g_return_if_fail(UFO_IS_GRAPH(user_data));
    UfoGraph *self = UFO_GRAPH(user_data);
    g_hash_table_insert(self->priv->property_sets,
            g_strdup(member_name),
            json_object_get_object_member(object, member_name));
}

static void graph_build(UfoGraph *self, JsonNode *root)
{
    JsonObject *root_object = json_node_get_object(root);

    if (json_object_has_member(root_object, "prop-sets")) {
        JsonObject *sets = json_object_get_object_member(root_object, "prop-sets");
        json_object_foreach_member(sets, graph_handle_json_propset, self);
    }

    if (json_object_has_member(root_object, "nodes")) {
        JsonArray *nodes = json_object_get_array_member(root_object, "nodes"); 
        json_array_foreach_element(nodes, graph_handle_json_filter_node, self);

        /* We only check edges if we have nodes, anything else doesn't make much
         * sense. */
        if (json_object_has_member(root_object, "edges")) {
            JsonArray *edges = json_object_get_array_member(root_object, "edges"); 
            json_array_foreach_element(edges, graph_handle_json_filter_edge, self);
        }
    }
}

static void graph_transfer_props_to_json_object(UfoFilter *filter, JsonObject *json_object)
{
    GObjectClass *klass = G_OBJECT_GET_CLASS(filter);
    guint num_properties = 0;

    GParamSpec **param_specs = g_object_class_list_properties(klass, &num_properties);
    GParamSpec *spec = param_specs[0];
    GValue value = {0, };

    for (guint i = 0; i < num_properties; i++, spec = param_specs[i]) {
        JsonNode *prop_node = json_node_new(JSON_NODE_VALUE);
        /* XXX: This is a stupid hack, because json-glib is unable to use guint
         * types, which is fairly common among our filters. To "fix" this, we
         * simply use gint. */
        GType value_type = spec->value_type == G_TYPE_UINT ? G_TYPE_INT : spec->value_type;

        g_value_init(&value, value_type);
        g_object_get_property(G_OBJECT(filter), spec->name, &value);

        /* We should ignore values that are default anyway */
        json_node_set_value(prop_node, &value);
        json_object_set_member(json_object, spec->name, prop_node);
        g_value_unset(&value);
    }

    g_free(param_specs);
}

static void graph_add_filter_to_json_array(gpointer data, gpointer user_data)
{
    g_return_if_fail(UFO_IS_FILTER(data));

    UfoFilter *filter = UFO_FILTER(data);
    JsonArray *array = (JsonArray *) user_data;
    JsonObject *node_object = json_object_new();
    JsonObject *prop_object = json_object_new();
    const gchar *plugin_name = ufo_filter_get_plugin_name(filter);

    graph_transfer_props_to_json_object(filter, prop_object);

    json_object_set_string_member(node_object, "name", plugin_name);
    json_object_set_string_member(node_object, "plugin", plugin_name);
    json_object_set_object_member(node_object, "properties", prop_object);
    json_array_add_object_element(array, node_object);
}

static gpointer graph_process_thread(gpointer data)
{
    ufo_filter_process(UFO_FILTER(data));
    return NULL;
}

static void graph_check_consistency(UfoGraphPrivate *priv)
{
    /* Build adjacency matrix */
    GList *elements = g_hash_table_get_values(priv->nodes);
    guint n = g_list_length(elements);
    UfoFilter *filters[n]; /* mapping from UfoFilter to N */
    int connections[n][n];  /* adjacency matrix */
    int out_degree[n], in_degree[n];

    for (guint i = 0; i < n; i++) {
        filters[i] = UFO_FILTER(g_list_nth_data(elements, i));
        in_degree[i] = 0;
        out_degree[i] = 0;
    }

    for (guint from = 0; from < n; from++) {
        UfoFilter *source = UFO_FILTER(g_list_nth_data(elements, from));

        for (guint to = 0; to < n; to++) {
            UfoFilter *dest = UFO_FILTER(g_list_nth_data(elements, to));
            connections[from][to] = ufo_filter_connected(source, dest) ? 1 : 0;
        }
    }

    for (guint from = 0; from < n; from++) {
        for (guint to = 0; to < n; to++) {
            out_degree[from] += connections[from][to];
            in_degree[from] += connections[to][from];
        }
    }

    /* Use the graph for statical analysis */
    for (guint i = 0; i < n; i++) {
        if (in_degree[i] == 0 && out_degree[i] == 0)
            g_warning("%s is not connected to any other filter", ufo_filter_get_plugin_name(filters[i]));
    }
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
UfoGraph *ufo_graph_new(const gchar *paths)
{
    return UFO_GRAPH(g_object_new(UFO_TYPE_GRAPH, NULL));
}

/**
 * ufo_graph_read_from_json:
 * @graph: A #UfoGraph.
 * @filename: Path and filename to the JSON file
 * @error: Indicates error in case of failed file loading or parsing
 *
 * Read a JSON configuration file to fill the filter structure of @graph.
 */
void ufo_graph_read_from_json(UfoGraph *graph, const gchar *filename, GError **error)
{
    g_return_if_fail(UFO_IS_GRAPH(graph) || (filename != NULL));
    JsonParser *json_parser = json_parser_new();
    GError *tmp_error = NULL;

    if (!json_parser_load_from_file(json_parser, filename, &tmp_error)) {
        g_propagate_error(error, tmp_error); 
        return;
    }

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
void ufo_graph_save_to_json(UfoGraph *graph, const gchar *filename, GError **error)
{
    g_return_if_fail(UFO_IS_GRAPH(graph) || (filename != NULL));

    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(graph);
    GError *tmp_error = NULL;
    GList *filters = g_hash_table_get_values(priv->nodes);
    const guint num_filters = g_list_length(filters);

    JsonGenerator *json_generator = json_generator_new();
    JsonNode *root_node = json_node_new(JSON_NODE_OBJECT);
    JsonObject *root_object = json_object_new();
    JsonArray *nodes = json_array_new();
    JsonArray *edges = json_array_new();

    g_list_foreach(filters, graph_add_filter_to_json_array, nodes);

    /* FIXME: I know, this is O(n^2) and is not covering the general case of named
     * inputs and outputs but it's something to begin with. */
    for (guint i = 0; i < num_filters; i++) {
        for (guint j = 0; j < num_filters; j++) {
            UfoFilter *from = g_list_nth_data(filters, i);
            UfoFilter *to = g_list_nth_data(filters, j);

            if (ufo_filter_connected(from, to)) {
                JsonObject *connect_object = json_object_new(); 
                json_object_set_string_member(connect_object, "from", ufo_filter_get_plugin_name(from));
                json_object_set_string_member(connect_object, "to", ufo_filter_get_plugin_name(to));
                json_array_add_object_element(edges, connect_object);
            }
        } 
    }

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
    g_return_if_fail(UFO_IS_GRAPH(graph));
    
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(graph);
    graph_check_consistency(priv);

    /* Assign GPUs to filters */
    cl_command_queue *cmd_queues;
    guint num_queues;
    ufo_resource_manager_get_command_queues(graph->priv->resource_manager, (void **) &cmd_queues, &num_queues);

    g_thread_init(NULL);
    GTimer *timer = g_timer_new();
    g_timer_start(timer);

    /* Start each filter in its own thread */
    GList *filter_root = g_hash_table_get_values(priv->nodes);
    GSList *threads = NULL;
    GError *tmp_error = NULL;

    for (GList *it = filter_root; it != NULL; it = g_list_next(it)) {
        UfoFilter *filter= UFO_FILTER(it->data);

        /* Using the same GPU for now */
        ufo_filter_set_command_queue(filter, cmd_queues[0]);
        threads = g_slist_append(threads,
                g_thread_create(graph_process_thread, filter, TRUE, &tmp_error));

        if (tmp_error != NULL) {
            /* FIXME: kill already started threads */
            g_propagate_error(error, tmp_error);
            return;
        }
    }

    g_list_free(filter_root);
    GSList *thread = threads;

    while (thread) {
        tmp_error = (GError *) g_thread_join(thread->data);
        if (tmp_error != NULL) {
            /* FIXME: wait for the rest? */
            g_propagate_error(error, tmp_error);
            return;
        }
        thread = g_slist_next(thread);
    }

    g_slist_free(threads);
    g_timer_stop(timer);
    g_message("Processing finished after %3.5f seconds", g_timer_elapsed(timer, NULL));
    g_timer_destroy(timer);
}

/**
 * ufo_graph_get_filter_names:
 * @graph: A #UfoGraph
 *
 * Return value: (element-type utf8) (transfer none): list of constants.
 */
GList *ufo_graph_get_filter_names(UfoGraph *graph)
{
    g_return_val_if_fail(UFO_IS_GRAPH(graph), NULL);
    return NULL;
}

/**
 * ufo_graph_get_filter:
 * @graph: a #UfoGraph
 * @plugin_name: name of the plugin
 * @error: return location for a GError with error codes from
 * #UfoPluginManagerError or %NULL
 *
 * Instantiate a new filter from a given plugin.
 *
 * Return value: (transfer full): a #UfoFilter
 */
UfoFilter *ufo_graph_get_filter(UfoGraph *graph, const gchar *plugin_name, GError **error)
{
    g_return_val_if_fail(UFO_IS_GRAPH(graph) || (plugin_name != NULL), NULL);
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(graph);
    GError *tmp_error = NULL;

    UfoFilter *filter = ufo_plugin_manager_get_filter(priv->plugin_manager, plugin_name, &tmp_error);
    g_object_ref(filter);

    if (tmp_error != NULL) {
        g_propagate_error(error, tmp_error);
        return NULL;
    }

    gchar *unique_name = g_strdup_printf("%s-%p", plugin_name, (void *) filter);
    ufo_graph_add_filter(graph, filter, unique_name);
    g_free(unique_name);
    return filter;
}

/**
 * ufo_graph_add_filter:
 * @graph: A #UfoGraph
 * @filter: A filter that the graph should care for
 * @name: A unique human-readable name
 *
 * In the case that a filter was not created using ufo_graph_get_filter() but in
 * a different place, you have to register the filter with this method. 
 *
 * Note: Once you have added a filter, you cannot unref the filter on your own.
 */
void ufo_graph_add_filter(UfoGraph *graph, UfoFilter *filter, const char *name)
{
    g_return_if_fail(UFO_IS_GRAPH(graph) || UFO_IS_FILTER(filter) || (name != NULL));
    /* FIXME: if the same filter type is added more than once, this won't work! */
    g_hash_table_insert(graph->priv->nodes, g_strdup(name), filter);
    ufo_filter_initialize(filter, name);
}

/**
 * ufo_graph_get_number_of_devices:
 * @graph: A #UfoGraph
 *
 * Query the number of used acceleration devices such as GPUs
 *
 * Return value: Number of devices
 */
guint ufo_graph_get_number_of_devices(UfoGraph *graph)
{
    g_return_val_if_fail(UFO_IS_GRAPH(graph), 0);
    return ufo_resource_manager_get_number_of_devices(graph->priv->resource_manager);
}

static void ufo_graph_dispose(GObject *object)
{
    UfoGraph *self = UFO_GRAPH(object);
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(self);

    g_object_unref(priv->plugin_manager);
    g_object_unref(priv->resource_manager);

    priv->plugin_manager = NULL;
    priv->resource_manager = NULL;
    G_OBJECT_CLASS(ufo_graph_parent_class)->dispose(object);
}

static void ufo_graph_finalize(GObject *object)
{
    UfoGraph *self = UFO_GRAPH(object);
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(self);

    g_hash_table_destroy(priv->property_sets);
    g_hash_table_destroy(priv->nodes);
    g_free(priv->paths);

    priv->property_sets = NULL;
    priv->paths = NULL;
    G_OBJECT_CLASS(ufo_graph_parent_class)->finalize(object);
}

static GObject *ufo_graph_constructor(GType gtype, guint n_properties, GObjectConstructParam *properties)
{
    GObject *object = G_OBJECT_CLASS(ufo_graph_parent_class)->constructor(gtype, n_properties, properties);

    if (!object)
        return NULL;

    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(object);
    gchar *paths = g_strdup_printf("%s:%s", priv->paths, LIB_FILTER_DIR);
    ufo_resource_manager_add_paths(priv->resource_manager, paths);

    priv->plugin_manager = ufo_plugin_manager_new();
    ufo_plugin_manager_add_paths(priv->plugin_manager, paths);
    g_free(paths);
    return object;
}

static void ufo_graph_set_property(GObject *object,
                                   guint           property_id,
                                   const GValue    *value,
                                   GParamSpec      *pspec)
{
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_PATHS:
            g_free(priv->paths);
            priv->paths = g_strdup(g_value_get_string(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_graph_get_property(GObject *object,
                                   guint       property_id,
                                   GValue      *value,
                                   GParamSpec  *pspec)
{
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_PATHS:
            g_value_set_string(value, priv->paths);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void ufo_graph_class_init(UfoGraphClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = ufo_graph_set_property;
    gobject_class->get_property = ufo_graph_get_property;
    gobject_class->dispose = ufo_graph_dispose;
    gobject_class->finalize = ufo_graph_finalize;
    gobject_class->constructor = ufo_graph_constructor;

    /**
     * UfoGraph:paths:
     *
     * List of colon-separated paths pointing to possible filter and kernel file
     * locations.
     */
    graph_properties[PROP_PATHS] =
        g_param_spec_string("paths",
                "List of :-separated paths pointing to possible filter locations",
                "List of :-separated paths pointing to possible filter locations",
                ".",
                G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_PATHS, graph_properties[PROP_PATHS]);
    g_type_class_add_private(klass, sizeof(UfoGraphPrivate));
}

static void ufo_graph_init(UfoGraph *self)
{
    UfoGraphPrivate *priv;
    self->priv = priv = UFO_GRAPH_GET_PRIVATE(self);
    priv->resource_manager = ufo_resource_manager();
    priv->property_sets = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    priv->nodes = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
    priv->paths = NULL;
}

