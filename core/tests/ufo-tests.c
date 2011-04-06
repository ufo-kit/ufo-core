
#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "ufo-graph.h"
#include "ufo-filter.h"

START_TEST (ufo_create)
{
    g_type_init();
    UfoGraph *graph = ufo_graph_new();
    fail_if(graph == NULL);
    UfoFilter *uca_reader = ufo_graph_create_node(graph, "uca");
    UfoFilter *histogram = ufo_graph_create_node(graph, "histogram");
    ufo_graph_set_root(graph, uca_reader);
    ufo_graph_connect(graph, uca_reader, histogram);
    ufo_graph_run(graph);
    g_object_unref(graph);
}
END_TEST


int main(int argc, char *argv[])
{
    printf("\n=== Using Check for Unit Tests =======================\n");
    Suite *s = suite_create("UFO");

    TCase *tc_devices = tcase_create("Devices");
    tcase_add_test(tc_devices, ufo_create);

    suite_add_tcase(s, tc_devices);

    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int fails = srunner_ntests_failed(sr);
    srunner_free(sr);
    printf("\n=== Finished Check ===================================\n");

    return (fails == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
