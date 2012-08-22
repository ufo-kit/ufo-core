
#include <stdlib.h>
#include <glib-object.h>
#include "ufo-configuration.h"
#include "ufo-graph.h"
#include "ufo-base-scheduler.h"

static void
handle_error (const gchar *prefix, GError *error, UfoGraph *graph)
{
    if (error) {
        g_error ("%s: %s", prefix, error->message);
        g_error_free (error);
        g_object_unref (graph);
        exit (EXIT_FAILURE);
    }
}

static GValueArray *
split_string (const gchar *string)
{
    GValueArray *array;
    gchar **components;
    GValue  value = {0};

    components = g_strsplit (string, ":", -1);
    array = g_value_array_new (2);
    g_value_init (&value, G_TYPE_STRING);

    for (guint i = 0; components[i] != NULL; i++) {
        g_value_reset (&value);
        g_value_set_string (&value, components[i]);
        g_value_array_append (array, &value);
    }

    g_strfreev (components);
    return array;
}

int main(int argc, char const* argv[])
{
    UfoConfiguration *config;
    UfoGraph         *graph;
    UfoBaseScheduler *scheduler;
    UfoPluginManager *manager;
    GError *error = NULL;

    g_type_init();

    if (argc < 2) {
        g_print("Usage: runjson FILE [FILTER-PATHS]\n");
        return 0;
    }

    config = ufo_configuration_new ();
    graph = ufo_graph_new ();

    if (argc == 2) {
        manager = ufo_plugin_manager_new (NULL);
    }
    else {
        GValueArray *paths;

        paths = split_string (argv[2]);

        g_object_set (G_OBJECT (config),
                      "paths", paths,
                      NULL);

        manager = ufo_plugin_manager_new (config);
        g_value_array_free (paths);
    }

    ufo_graph_read_from_json(graph, manager, argv[1], &error);
    handle_error("Reading JSON", error, graph);

    scheduler = ufo_base_scheduler_new (config, NULL);
    ufo_base_scheduler_run (scheduler, graph, &error);
    handle_error("Executing", error, graph);

    g_object_unref(graph);
    return 0;
}
