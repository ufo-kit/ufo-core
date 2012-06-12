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
    UfoResourceManager  *resource_manager;
    UfoBaseScheduler    *scheduler;
    GHashTable          *property_sets;  /**< maps from gchar* to JsonNode */
    gchar               *paths;
    GList               *relations;
    GList               *filters;
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

/*     UfoGraph *self = UFO_GRAPH(user_data); */
/*     UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(self); */

/*     JsonObject *object = json_node_get_object(node); */
/*     /1* TODO: check existence *1/ */
/*     const gchar *plugin_name = json_object_get_string_member(object, "plugin"); */
/*     const gchar *node_name = json_object_get_string_member(object, "name"); */
/*     /1* UfoFilter *filter = ufo_graph_get_filter(self, plugin_name, NULL); *1/ */

/*     /1* TODO: check that the node name is unique *1/ */

/*     if (filter == NULL) { */
/*         g_warning("Couldn't find plugin '%s'", plugin_name); */
/*         return; */
/*     } */

/*     /1* We can define "properties" for each filter ... *1/ */
/*     if (json_object_has_member(object, "properties")) { */
/*         JsonObject *prop_object = json_object_get_object_member(object, "properties"); */
/*         json_object_foreach_member(prop_object, */
/*                                    graph_handle_json_single_prop, */
/*                                    filter); */
/*     } */

/*     /1* ... and also add more through prop-refs *1/ */
/*     if (json_object_has_member(object, "prop-refs")) { */
/*         JsonArray *prop_refs = json_object_get_array_member(object, "prop-refs"); */

/*         for (guint i = 0; i < json_array_get_length(prop_refs); i++) { */
/*             const gchar *set_name = json_array_get_string_element(prop_refs, i); */
/*             JsonObject *prop_set = g_hash_table_lookup(priv->property_sets, set_name); */

/*             if (prop_set == NULL) */
/*                 g_warning("No property set '%s' in 'prop-sets'", set_name); */
/*             else { */
/*                 json_object_foreach_member(prop_set, */
/*                         graph_handle_json_single_prop, */
/*                         filter); */
/*             } */
/*         } */

/*         json_array_unref(prop_refs); */
/*     } */
}

static void graph_handle_json_filter_edge(JsonArray *array, guint index, JsonNode *node, gpointer user_data)
{
    g_return_if_fail(UFO_IS_GRAPH(user_data));
    /* UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(user_data); */
    /* JsonObject *object = json_node_get_object(node); */
    /* const gchar *from_name = json_object_get_string_member(object, "from"); */
    /* const gchar *to_name = json_object_get_string_member(object, "to"); */
    /* UfoFilter *from = g_hash_table_lookup(priv->nodes, from_name); */
    /* UfoFilter *to = g_hash_table_lookup(priv->nodes, to_name); */

    /* TODO: create relation here */
    /* ufo_filter_connect_to(from, to, NULL); */
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

static void graph_check_consistency(UfoGraphPrivate *priv)
{
    /* /1* Build adjacency matrix *1/ */
    /* guint n = g_list_length(elements); */
    /* UfoFilter *filters[n]; /1* mapping from UfoFilter to N *1/ */
    /* int connections[n][n];  /1* adjacency matrix *1/ */
    /* int out_degree[n], in_degree[n]; */

    /* for (guint i = 0; i < n; i++) { */
    /*     filters[i] = UFO_FILTER(g_list_nth_data(elements, i)); */
    /*     in_degree[i] = 0; */
    /*     out_degree[i] = 0; */
    /* } */

    /* for (guint from = 0; from < n; from++) { */
    /*     UfoFilter *source = UFO_FILTER(g_list_nth_data(elements, from)); */

    /*     for (guint to = 0; to < n; to++) { */
    /*         UfoFilter *dest = UFO_FILTER(g_list_nth_data(elements, to)); */
    /*         /1* FIXME: we need to check this *1/ */
    /*         /1* connections[from][to] = ufo_filter_connected(source, dest) ? 1 : 0; *1/ */
    /*     } */
    /* } */

    /* for (guint from = 0; from < n; from++) { */
    /*     for (guint to = 0; to < n; to++) { */
    /*         out_degree[from] += connections[from][to]; */
    /*         in_degree[from] += connections[to][from]; */
    /*     } */
    /* } */

    /* /1* Use the graph for statical analysis *1/ */
    /* for (guint i = 0; i < n; i++) { */
    /*     if (in_degree[i] == 0 && out_degree[i] == 0) */
    /*         g_warning("%s is not connected to any other filter", ufo_filter_get_plugin_name(filters[i])); */
    /* } */
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
    return UFO_GRAPH(g_object_new(UFO_TYPE_GRAPH, "paths", paths, NULL));
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
    /* g_return_if_fail(UFO_IS_GRAPH(graph) || (filename != NULL)); */

    /* UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE(graph); */
    /* GError *tmp_error = NULL; */
    /* const guint num_filters = g_list_length(filters); */

    /* JsonGenerator *json_generator = json_generator_new(); */
    /* JsonNode *root_node = json_node_new(JSON_NODE_OBJECT); */
    /* JsonObject *root_object = json_object_new(); */
    /* JsonArray *nodes = json_array_new(); */
    /* JsonArray *edges = json_array_new(); */

    /* g_list_foreach(filters, graph_add_filter_to_json_array, nodes); */

    /* /1* FIXME: I know, this is O(n^2) and is not covering the general case of named */
    /*  * inputs and outputs but it's something to begin with. *1/ */
    /* for (guint i = 0; i < num_filters; i++) { */
    /*     for (guint j = 0; j < num_filters; j++) { */
    /*         UfoFilter *from = g_list_nth_data(filters, i); */
    /*         UfoFilter *to = g_list_nth_data(filters, j); */

    /*         /1* FIXME: we need to check this *1/ */
    /*         /1* if (ufo_filter_connected(from, to)) { *1/ */
    /*             JsonObject *connect_object = json_object_new(); */ 
    /*             json_object_set_string_member(connect_object, "from", ufo_filter_get_plugin_name(from)); */
    /*             json_object_set_string_member(connect_object, "to", ufo_filter_get_plugin_name(to)); */
    /*             json_array_add_object_element(edges, connect_object); */
    /*         /1* } *1/ */
    /*     } */ 
    /* } */

    /* json_object_set_array_member(root_object, "nodes", nodes); */
    /* json_object_set_array_member(root_object, "edges", edges); */
    /* json_node_set_object(root_node, root_object); */
    /* json_generator_set_root(json_generator, root_node); */

    /* if (!json_generator_to_file(json_generator, filename, &tmp_error)) { */
    /*     g_propagate_error(error, tmp_error); */ 
    /*     return; */
    /* } */

    /* g_object_unref(json_generator); */
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

    ufo_base_scheduler_run(priv->scheduler, priv->relations, error);
}

/**
 * ufo_graph_get_filter_names:
 * @graph: A #UfoGraph
 *
 * Enumerate all available filters that can be instatiated with
 * ufo_graph_get_filter().
 *
 * Return value: (element-type utf8) (transfer none): list of constants.
 */
GList *ufo_graph_get_filter_names(UfoGraph *graph)
{
    g_return_val_if_fail(UFO_IS_GRAPH(graph), NULL);
    return ufo_plugin_manager_available_filters(graph->priv->plugin_manager);
}

void ufo_graph_add_relation(UfoGraph *graph, UfoRelation *relation)
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
void ufo_graph_connect_filters (UfoGraph *graph, UfoFilter *from, UfoFilter *to, GError **error)
{
    UfoRelation *relation;
    GError      *tmp_error = NULL;

    g_return_if_fail (UFO_IS_GRAPH (graph) && UFO_IS_FILTER (from) && UFO_IS_FILTER (to));

    relation = ufo_relation_new (from, 0, UFO_RELATION_MODE_DISTRIBUTE);
    ufo_relation_add_consumer (relation, to, 0, &tmp_error);

    if (tmp_error == NULL) {
        /* 
         * We don't call ufo_graph_add_relation() because we don't want to
         * reference the relation object once again.
         */
        graph->priv->relations = g_list_append (graph->priv->relations, relation);
    }
    else
        g_propagate_error (error, tmp_error);
}

static void g_object_unref_safe (gpointer data, gpointer user_data)
{
    UfoFilter *filter = (UfoFilter *) data;

    if (UFO_IS_FILTER (filter))
        g_object_unref (filter);
}

static void ufo_graph_dispose(GObject *object)
{
    UfoGraph *self = UFO_GRAPH (object);
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE (self);

    g_list_foreach (priv->relations, (GFunc) g_object_unref, NULL);
    g_print ("kill filters\n");
    g_list_foreach (priv->filters, (GFunc) g_object_unref_safe, NULL);
    g_print ("done\n");

    /* g_object_unref (priv->plugin_manager); */
    g_object_unref (priv->resource_manager);
    g_object_unref (priv->scheduler);

    priv->plugin_manager = NULL;
    priv->resource_manager = NULL;
    G_OBJECT_CLASS (ufo_graph_parent_class)->dispose (object);
}

static void ufo_graph_finalize(GObject *object)
{
    UfoGraph *self = UFO_GRAPH (object);
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE (self);

    g_hash_table_destroy (priv->property_sets);
    g_list_free (priv->relations);
    g_list_free (priv->filters);
    g_free (priv->paths);

    priv->property_sets = NULL;
    priv->paths = NULL;
    G_OBJECT_CLASS (ufo_graph_parent_class)->finalize (object);
}

static void ufo_graph_constructed(GObject *object)
{
    UfoGraphPrivate *priv = UFO_GRAPH_GET_PRIVATE (object);
    gchar *paths = g_strdup_printf ("%s:%s", priv->paths, LIB_FILTER_DIR);

    ufo_resource_manager_add_paths (priv->resource_manager, paths);
    /* ufo_plugin_manager_add_paths (priv->plugin_manager, paths); */

    if (G_OBJECT_CLASS (ufo_graph_parent_class)->constructed != NULL)
        G_OBJECT_CLASS (ufo_graph_parent_class)->constructed (object);

    g_free (paths);
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
    gobject_class->constructed = ufo_graph_constructed;

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
    self->priv = priv = UFO_GRAPH_GET_PRIVATE (self);
    priv->plugin_manager = NULL;
    priv->resource_manager = ufo_resource_manager ();
    priv->scheduler = ufo_base_scheduler_new ();
    priv->property_sets = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    priv->paths = NULL;
    priv->relations = NULL;
    priv->filters = NULL;
}

