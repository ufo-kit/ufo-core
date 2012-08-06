
#include <stdlib.h>
#include <glib.h>
#include "ufo-graph.h"

static void handle_error(GError *error, UfoGraph *graph)
{
    if (error) {
        g_error("%s", error->message);
        g_error_free(error);
        g_object_unref(graph);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char const* argv[])
{
    UfoGraph *graph;
    UfoPluginManager *manager;
    GError *error = NULL;

    g_type_init();

    if (argc < 2) {
        g_print("Usage: runjson FILE [FILTER-PATHS]\n");
        return 0;
    }

    if (argc == 2) {
        graph = ufo_graph_new("");
        manager = ufo_plugin_manager_new ("");
    }
    else {
        graph = ufo_graph_new(argv[2]);
        manager = ufo_plugin_manager_new (argv[2]);
    }

    ufo_graph_read_from_json(graph, manager, argv[1], &error);
    handle_error(error, graph);

    ufo_graph_run(graph, &error);
    handle_error(error, graph);

    g_object_unref(graph);
    return 0;
}
