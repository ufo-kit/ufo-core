#include <glib.h>
#include <ethos/ethos.h>
#include <json-glib/json-glib.h>

#include "ufo-graph.h"
#include "ufo-container.h"
#include "ufo-sequence.h"
#include "ufo-split.h"

G_DEFINE_TYPE(UfoGraph, ufo_graph, G_TYPE_OBJECT);

#define UFO_GRAPH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GRAPH, UfoGraphPrivate))

struct _UfoGraphPrivate {
    EthosManager        *ethos;
    UfoResourceManager  *resource_manager;
    UfoContainer        *root_container;
    GHashTable          *plugins;   /**< maps from gchar* to EthosPlugin* */
};

GList *ufo_graph_get_filter_names(UfoGraph *self)
{
    return g_hash_table_get_keys(self->priv->plugins);
}

UfoFilter *ufo_graph_create_node(UfoGraph *self, gchar *filter_name)
{
    EthosPlugin *plugin = g_hash_table_lookup(self->priv->plugins, filter_name);
    /* TODO: When we move to libpeas we have to instantiate new objects each
     * time a user requests a new stateful node. */
    if (plugin != NULL) {
        UfoFilter *filter = (UfoFilter *) plugin;
        ufo_filter_set_resource_manager(filter, self->priv->resource_manager);
        return filter;
    }
    return NULL;
}

UfoFilter *ufo_graph_get_filter(UfoGraph *self, const gchar *plugin_name)
{
    return (UfoFilter *) g_hash_table_lookup(self->priv->plugins, plugin_name);
}

void ufo_graph_build(UfoGraph *self, JsonNode *node, UfoContainer **container)
{
    JsonObject *object = json_node_get_object(node);
    if (json_object_get_member(object, "type")) {
        const char *type = json_object_get_string_member(object, "type");

        if (g_strcmp0(type, "filter") == 0) {
            /* TODO: pull out plugin-name and parameters and wire it into the
             * element */
            const gchar *plugin_name = json_object_get_string_member(object, "plugin");
            g_message("add filter '%s'", plugin_name);
            UfoFilter *filter = ufo_graph_get_filter(self, plugin_name);
            if (filter != NULL) {
                UfoElement *element = ufo_element_new();
                ufo_element_set_filter(element, filter);
                ufo_container_add_element(*container, element);
            }
            else
                g_message("Couldn't find plugin '%s'", plugin_name);
        }
        else {
            UfoContainer *new_container = NULL;
            if (g_strcmp0(type, "sequence") == 0) {
                g_message("add sequence node");
                new_container = ufo_sequence_new();
            }
            else if (g_strcmp0(type, "split") == 0) {
                g_message("add split node");
                new_container = ufo_split_new();
            }

            /* Neither filter, sequence or split... just return */
            if (new_container == NULL)
                return;
            
            /* If we have a root container, assign the newly created one */
            if (*container == NULL)
                *container = new_container;

            ufo_container_add_element(*container, (UfoElement *) new_container);
            JsonArray *elements = json_object_get_array_member(object, "elements");
            for (guint i = 0; i < json_array_get_length(elements); i++) 
                ufo_graph_build(self, json_array_get_element(elements, i), &new_container);
        }
    }
}

void ufo_graph_read_json_configuration(UfoGraph *self, GString *filename)
{
    static const char *config = 
        "{"
        "   \"type\" : \"sequence\","
        "   \"elements\" : ["
        "       { \"type\" : \"filter\", \"plugin\" : \"uca\" },"
        "       { \"type\" : \"sequence\", \"elements\" : ["
        "           { \"type\" : \"filter\", \"plugin\" : \"raw\" }"
        "       ] }"
        "   ]"
        "}\0";

    JsonParser *parser = json_parser_new();
    GError *error = NULL;
    json_parser_load_from_data(parser, config, -1, &error);
    if (error) {
        g_message("Parse error: %s", error->message);
        g_error_free(error);
        g_object_unref(parser);
        return;
    }

    ufo_graph_build(self, json_parser_get_root(parser), &self->priv->root_container);
    g_object_unref(parser);
}

void ufo_graph_run(UfoGraph *self)
{
}

UfoGraph *ufo_graph_new()
{
    return g_object_new(UFO_TYPE_GRAPH, NULL);
}

static void ufo_graph_dispose(GObject *gobject)
{
    UfoGraph *self = UFO_GRAPH(gobject);
    
    if (self->priv->plugins) {
        g_hash_table_destroy(self->priv->plugins);
        self->priv->plugins = NULL;
    }

    if (self->priv->resource_manager) {
        g_object_unref(self->priv->resource_manager);
        self->priv->resource_manager = NULL;
    }

    G_OBJECT_CLASS(ufo_graph_parent_class)->dispose(gobject);
}

static void ufo_graph_add_plugin(gpointer data, gpointer user_data)
{
    EthosPluginInfo *info = (EthosPluginInfo *) data;
    UfoGraphPrivate *priv = (UfoGraphPrivate *) user_data;

    g_message("Load filter: %s", ethos_plugin_info_get_name(info));

    g_hash_table_insert(priv->plugins, 
        (gpointer) ethos_plugin_info_get_name(info),
        ethos_manager_get_plugin(priv->ethos, info));
}

static void ufo_graph_class_init(UfoGraphClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose = ufo_graph_dispose;

    /* install private data */
    g_type_class_add_private(klass, sizeof(UfoGraphPrivate));
}

static void ufo_graph_init(UfoGraph *self)
{
    /* init public fields */

    /* init private fields */
    UfoGraphPrivate *priv;
    self->priv = priv = UFO_GRAPH_GET_PRIVATE(self);

    /* TODO: determine directories in a better way */
    gchar *plugin_dirs[] = { "/usr/local/lib", "../filters", NULL };

    priv->ethos = ethos_manager_new_full("UFO", plugin_dirs);
    ethos_manager_initialize(priv->ethos);

    priv->plugins = g_hash_table_new(g_str_hash, g_str_equal);
    GList *plugin_info = ethos_manager_get_plugin_info(priv->ethos);

    g_list_foreach(plugin_info, &ufo_graph_add_plugin, priv);
    g_list_free(plugin_info);

    priv->resource_manager = ufo_resource_manager_new();
    priv->root_container = NULL;
}


