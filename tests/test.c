
#include <glib.h>
#include "ufo-graph.h"

int main(int argc, char const* argv[])
{
    g_type_init();
    GError *error = NULL;
    if (argc < 2) {
        g_print("Usage: runjson FILE [FILTER-PATHS]\n");
        return 0;
    }
    UfoGraph *graph = NULL;
    if (argc == 2)
        graph = ufo_graph_new();
    else
        graph = g_object_new(UFO_TYPE_GRAPH, "paths", argv[2], NULL);

    ufo_graph_read_from_json(graph, argv[1], &error);

    if (error) {
        g_error("Error: %s", error->message);
        g_error_free(error);
        g_object_unref(graph);
        return 1;
    }
    ufo_graph_run(graph);
    g_object_unref(graph);
    return 0;
}
