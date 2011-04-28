
#include <glib.h>
#include "ufo-graph.h"

int main(int argc, char const* argv[])
{
    g_type_init();
    UfoGraph *graph = ufo_graph_new();
    ufo_graph_read_from_json(graph, "test.json");
    ufo_graph_run(graph);
    g_object_unref(graph);
    return 0;
}
