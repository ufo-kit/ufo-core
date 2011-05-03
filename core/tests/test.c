
#include <glib.h>
#include "ufo-graph.h"

int main(int argc, char const* argv[])
{
    g_type_init();
    GError *error = NULL;
    UfoGraph *graph = ufo_graph_new();
    ufo_graph_read_from_json(graph, "test.json", &error);
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
