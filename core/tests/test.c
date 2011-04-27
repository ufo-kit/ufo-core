
#include <glib.h>
#include "ufo-graph.h"

int main(int argc, char const* argv[])
{
    UfoGraph *graph = ufo_graph_new_from_json("test.json");
    ufo_graph_run(graph);
    return 0;
}
