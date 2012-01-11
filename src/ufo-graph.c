#include <glib.h>
#include <json-glib/json-glib.h>

#include "config.h"

#include "ufo-graph.h"
#include "ufo-sequence.h"
#include "ufo-split.h"
#include "ufo-resource-manager.h"
#include "ufo-element.h"
#include "ufo-plugin-manager.h"

G_DEFINE_TYPE(UfoGraph, ufo_graph, G_TYPE_OBJECT);

#define UFO_GRAPH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GRAPH, UfoGraphPrivate))

#define UFO_GRAPH_ERROR ufo_graph_error_quark()
enum UfoGraphError {
    UFO_GRAPH_ERROR_ALREADY_LOAD,
    UFO_GRAPH_ERROR_FILTER_NOT_FOUND
};

struct _UfoGraphPrivate {
    UfoPluginManager    *plugin_manager;
    JsonParser          *json_parser;
    UfoResourceManager  *resource_manager;
    GSList              *elements;
    GHashTable          *property_sets;  /**< maps from gchar* to JsonNode */
    gchar               *paths;
};

enum {
    PROP_0,
    PROP_PATHS,
    N_PROPERTIES
};

static GParamSpec *graph_properties[N_PROPERTIES] = { NULL, };


static void graph_handle_json_single_prop(JsonObject *object, const gchar *name, JsonNode *node, gpointer user)
{
    GValue val = { 0, };
    json_node_get_value(node, &val);
    g_object_set_property(G_OBJECT(user), name, &val);
    g_value_unset(&val);
}

static UfoFilter *graph_handle_json_filter(UfoGraph *self, JsonObject *object)
{
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(self);
    const gchar *plugin_name = json_object_get_string_member(object, "plugin");
    UfoFilter *filter = ufo_graph_get_filter(self, plugin_name, NULL);

    if (filter == NULL) {
        g_warning("Couldn't find plugin '%s'", plugin_name);
        return NULL;
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
            if (prop_set == NULL) {
                g_warning("No property set '%s' in 'prop-sets'", set_name);
            }
            else {
                json_object_foreach_member(prop_set,
                        graph_handle_json_single_prop,
                        filter);
            }
        }
    }
    return filter;
}

static void graph_handle_json_sequence(UfoGraph *self, JsonObject *sequence)
{
    if (json_object_has_member(sequence, "elements")) {
        UfoFilter *predecessor = NULL;
        JsonArray *elements = json_object_get_array_member(sequence, "elements");
        for (int i = 0; i < json_array_get_length(elements); i++) {
            JsonNode *node = json_array_get_element(elements, i);     
            UfoFilter *current = graph_handle_json_filter(self, json_node_get_object(node));

            /* Connect predecessor's output with current input */
            if (predecessor != NULL)
                ufo_filter_connect_to(predecessor, current);
            predecessor = current;
        }
    }
    else
        g_warning("Sequence has no <elements>");
}

static void graph_handle_json_type(UfoGraph *self, JsonObject *object)
{
    const char *type = json_object_get_string_member(object, "type");

    if (g_strcmp0(type, "filter") == 0)
        graph_handle_json_filter(self, object);
    else if (g_strcmp0(type, "sequence") == 0)
        graph_handle_json_sequence(self, object);
    else
        g_warning("Unknown type '%s'", type);
}

static void graph_handle_json_propset(JsonObject *object, 
    const gchar *member_name, 
    JsonNode *member_node, 
    gpointer user_data)
{
    UfoGraph *self = UFO_GRAPH(user_data);
    g_hash_table_insert(self->priv->property_sets, 
            (gpointer) g_strdup(member_name), 
            (gpointer) json_object_get_object_member(object, member_name));
}

static void graph_build(UfoGraph *self, JsonNode *node)
{
    JsonObject *object = json_node_get_object(node);
    if (json_object_has_member(object, "prop-sets")) {
        JsonObject *sets = json_object_get_object_member(object, "prop-sets");
        json_object_foreach_member(sets, graph_handle_json_propset, self);
    }
    if (json_object_has_member(object, "type")) {
        graph_handle_json_type(self, object);
    }
}

static gpointer graph_process_thread(gpointer data)
{
    ufo_filter_process(UFO_FILTER(data));
    return NULL;
}

static void graph_join_thread(gpointer data, gpointer user_data)
{
    GThread *thread = (GThread *) data;
    g_thread_join(thread);
}


GQuark ufo_graph_error_quark(void)
{
    return g_quark_from_static_string("ufo-graph-error-quark");
}


/**
 * ufo_graph_new:
 * Create a new #UfoGraph.
 *
 * Because resources (especially those belonging to the GPU) should only be
 * allocated once, we allow only one graph at a time. Thus the graph is a
 * singleton.
 *
 * Return value: A #UfoGraph.
 */
UfoGraph *ufo_graph_new(void)
{
    static UfoGraph *graph = NULL;
    if (graph == NULL)
        graph = UFO_GRAPH(g_object_new(UFO_TYPE_GRAPH, NULL));
    return graph;
}

/**
 * ufo_graph_read_from_json:
 * @graph: A #UfoGraph.
 * @filename: Path and filename to the JSON file
 * @error: Indicates error in case of failed file loading or parsing
 *
 * Read a JSON configuration file to fill the filter structure of #UfoGraph.
 */
void ufo_graph_read_from_json(UfoGraph *graph, const gchar *filename, GError **error)
{
    *error = NULL;
    json_parser_load_from_file(graph->priv->json_parser, filename, error);
    if (*error)
        return;

    graph_build(graph, json_parser_get_root(graph->priv->json_parser));
}

/**
 * ufo_graph_run:
 * @graph: A #UfoGraph.
 *
 * Start execution of all UfoElements in the UfoGraph until no more data is
 * produced
 */
void ufo_graph_run(UfoGraph *graph)
{
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(graph);
    
    /* Build adjacency matrix */
    int n = g_slist_length(priv->elements);
    UfoFilter *filters[n]; /* mapping from UfoFilter to N */
    int connections[n][n];  /* adjacency matrix */
    int out_degree[n], in_degree[n];
    
    for (int i = 0; i < n; i++) {
        filters[i] = UFO_FILTER(g_slist_nth_data(priv->elements, i));
        in_degree[i] = 0;
        out_degree[i] = 0;
    }
    
    for (int from = 0; from < n; from++) {
        UfoFilter *source = UFO_FILTER(g_slist_nth_data(priv->elements, from)); 
        for (int to = 0; to < n; to++) {
            UfoFilter *dest = UFO_FILTER(g_slist_nth_data(priv->elements, to)); 
            connections[from][to] = ufo_filter_connected(source, dest) ? 1 : 0;
        }
    }
    
    for (int from = 0; from < n; from++) {
        for (int to = 0; to < n; to++) {
            out_degree[from] += connections[from][to];
            in_degree[from] += connections[to][from];
        }
    }
    
    /* Use the graph for statical analysis */
    for (int i = 0; i < n; i++) {
        if (in_degree[i] == 0 && out_degree[i] == 0)
            g_error("Filter %i is not connected to any other filter", i);
    }
    
    /* Assign GPUs to filters */
    cl_command_queue *cmd_queues;
    int num_queues;
    ufo_resource_manager_get_command_queues(graph->priv->resource_manager, (void **) &cmd_queues, &num_queues); 

    /* Single GPU assignment */
    for (int i = 0; i < n; i++) {
        ufo_filter_set_command_queue(filters[i], cmd_queues[0]);
    }
    
    g_thread_init(NULL);
    GTimer *timer = g_timer_new();
    g_timer_start(timer);

    /* Start each filter in its own thread */
    GList *threads = NULL;
    GError *error = NULL;
    for (guint i = 0; i < g_slist_length(priv->elements); i++) {
        UfoFilter *child = UFO_FILTER(g_slist_nth_data(priv->elements, i));
        threads = g_list_append(threads,
                g_thread_create(graph_process_thread, child, TRUE, &error));
    }
    g_list_foreach(threads, graph_join_thread, NULL);
    g_list_free(threads);

    g_timer_stop(timer);
    g_message("Processing finished after %3.5f seconds", 
            g_timer_elapsed(timer, NULL));
    g_timer_destroy(timer);
}

/**
 * ufo_graph_get_filter_names:
 * 
 * Return value: (element-type utf8) (transfer none): list of constants.
 */
GList *ufo_graph_get_filter_names(UfoGraph *graph)
{
    return NULL;
}

/**
 * ufo_graph_get_filter:
 * @graph: a #UfoGraph
 * @plugin_name: name of the plugin
 * @error: return location for a GError or NULL
 *
 * Instantiate a new filter from a given plugin.
 *
 * Return value: (transfer full): a #UfoFilter
 */
UfoFilter *ufo_graph_get_filter(UfoGraph *graph, const gchar *plugin_name, GError **error)
{
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(graph);
    UfoFilter *filter = ufo_plugin_manager_get_filter(priv->plugin_manager, plugin_name);
    if (filter == NULL) {
        g_set_error(error,
                UFO_GRAPH_ERROR,
                UFO_GRAPH_ERROR_FILTER_NOT_FOUND,
                "Filter 'libfilter%s.so' not found",
                plugin_name);
        return NULL;
    }
    ufo_filter_initialize(filter, plugin_name);
    priv->elements = g_slist_prepend(priv->elements, filter);

    return filter;
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
    return ufo_resource_manager_get_number_of_devices(graph->priv->resource_manager);
}

static void ufo_graph_dispose(GObject *object)
{
    UfoGraph *self = UFO_GRAPH(object);
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(self);

    g_object_unref(priv->json_parser);
    g_object_unref(priv->plugin_manager);
    g_object_unref(priv->resource_manager);

    priv->json_parser = NULL;
    priv->plugin_manager = NULL;
    priv->resource_manager = NULL;

    g_slist_foreach(priv->elements, (GFunc) g_object_unref, NULL);

    G_OBJECT_CLASS(ufo_graph_parent_class)->dispose(object);
}

static void ufo_graph_finalize(GObject *object)
{
    UfoGraph *self = UFO_GRAPH(object);
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(self);

    g_hash_table_destroy(priv->property_sets);
    g_free(priv->paths);

    priv->property_sets = NULL;
    priv->paths = NULL;

    g_slist_free(priv->elements);

    G_OBJECT_CLASS(ufo_graph_parent_class)->finalize(object);
}

static GObject *ufo_graph_constructor(GType gtype, guint n_properties, GObjectConstructParam *properties)
{
    GObject *object = G_OBJECT_CLASS(ufo_graph_parent_class)->constructor(gtype, n_properties, properties);
    if (!object)
        return NULL;

    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(object);

    gchar *paths = g_strdup_printf("%s:%s", LIB_FILTER_DIR, priv->paths);
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
    gobject_class->dispose = ufo_graph_finalize;
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

    priv->elements = NULL;
    priv->json_parser = json_parser_new();
    priv->property_sets = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    priv->paths = NULL;
}


