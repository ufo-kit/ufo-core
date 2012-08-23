#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
#include "test-suite.h"
#include "ufo-graph.h"
#include "ufo-plugin-manager.h"
#include "ufo-filter-source.h"

typedef struct {
    UfoGraph         *graph;
    UfoPluginManager *manager;
    gchar            *valid_json_file;
    gchar            *invalid_json_file;
    gchar            *empty_json_file;
    UfoFilter        *source;
    UfoFilter        *sink1;
    UfoFilter        *sink2;
} Fixture;

static void
write_file (const gchar *name, const gchar *content)
{
    FILE *fp;
    fp = fopen (name, "w");
    fwrite (content, 1, strlen (content), fp);
    fclose (fp);
}

static void
fixture_json_setup (Fixture *fixture, gconstpointer data)
{
    static const gchar *valid_json =
        "{\
            \"nodes\": [{\"plugin\": \"foobarSie8Shoh\", \"name\": \"foobar\"}],\
            \"edges\": []\
        }";
    static const gchar *invalid_json = "{\"nodes:[]}"; /* missing " */
    static const gchar *empty_json = "{\"nodes\": [{}]}";

    fixture->graph = ufo_graph_new ();
    g_assert (UFO_IS_GRAPH (fixture->graph));

    fixture->manager = ufo_plugin_manager_new (NULL);
    g_assert (UFO_IS_PLUGIN_MANAGER (fixture->manager));

    fixture->valid_json_file = g_strdup_printf ("valid.json");
    fixture->invalid_json_file = g_strdup_printf ("invalid.json");
    fixture->empty_json_file = g_strdup_printf ("empty.json");

    write_file (fixture->valid_json_file, valid_json);
    write_file (fixture->invalid_json_file, invalid_json);
    write_file (fixture->empty_json_file, empty_json);
}

static void
fixture_json_teardown (Fixture *fixture, gconstpointer data)
{
    g_remove (fixture->valid_json_file);
    g_remove (fixture->invalid_json_file);
    g_remove (fixture->empty_json_file);

    g_free (fixture->valid_json_file);
    g_free (fixture->invalid_json_file);
    g_free (fixture->empty_json_file);

    g_object_unref (fixture->graph);
    g_object_unref (fixture->manager);
}

static void
fixture_filter_setup (Fixture *fixture, gconstpointer data)
{
    UfoInputParameter input_params[] = {{2, UFO_FILTER_INFINITE_INPUT}};
    UfoOutputParameter output_params[] = {{2}};

    GError *error = NULL;

    fixture->graph = ufo_graph_new ();
    g_assert (UFO_IS_GRAPH (fixture->graph));

    fixture->source = UFO_FILTER (g_object_new (UFO_TYPE_FILTER_SOURCE, NULL));
    fixture->sink1 = UFO_FILTER (g_object_new (UFO_TYPE_FILTER, NULL));
    fixture->sink2 = UFO_FILTER (g_object_new (UFO_TYPE_FILTER, NULL));

    ufo_filter_register_outputs (fixture->source, 1, output_params);
    ufo_filter_register_inputs (fixture->sink1, 1, input_params);
    ufo_filter_register_inputs (fixture->sink2, 1, input_params);

    ufo_graph_connect_filters (fixture->graph, fixture->source, fixture->sink1, &error);
    g_assert_no_error (error);

    ufo_graph_connect_filters (fixture->graph, fixture->source, fixture->sink2, &error);
    g_assert_no_error (error);
}

static void
fixture_filter_teardown (Fixture *fixture, gconstpointer data)
{
    g_object_unref (fixture->graph);
    g_object_unref (fixture->source);
    g_object_unref (fixture->sink1);
    g_object_unref (fixture->sink2);
}

static void
test_json_file_not_found (Fixture *fixture, gconstpointer data)
{
    GError *error = NULL;

    ufo_graph_read_from_json (fixture->graph,
                              fixture->manager,
                              "foobar.json",
                              &error);

    g_assert (error != NULL);
    g_error_free (error);
}

static void
test_json_invalid_json (Fixture *fixture, gconstpointer data)
{
    GError *error = NULL;

    ufo_graph_read_from_json (fixture->graph,
                              fixture->manager,
                              fixture->invalid_json_file,
                              &error);

    g_assert (error != NULL);
    g_error_free (error);
}

static void
test_json_read_unknown_filter (Fixture *fixture, gconstpointer data)
{
    GError *error = NULL;

    ufo_graph_read_from_json (fixture->graph,
                              fixture->manager,
                              fixture->valid_json_file,
                              &error);

    g_assert (error != NULL);
    g_error_free (error);
}

static void
test_json_key_not_found (Fixture *fixture, gconstpointer data)
{
    GError *error = NULL;

    ufo_graph_read_from_json (fixture->graph,
                              fixture->manager,
                              fixture->empty_json_file,
                              &error);

    g_assert (error != NULL);
    g_assert (error->code == UFO_GRAPH_ERROR_JSON_KEY);
    g_error_free (error);
}

static void
test_get_filters (Fixture *fixture, gconstpointer data)
{
    GList *filters;

    filters = ufo_graph_get_filters (fixture->graph);
    g_assert (filters != NULL);
    g_assert (g_list_length (filters) == 3);
    g_list_free (filters);
}

static void
test_get_roots (Fixture *fixture, gconstpointer data)
{
    GList *roots;

    roots = ufo_graph_get_roots (fixture->graph);
    g_assert (g_list_length (roots) == 1);
    g_assert (g_list_nth_data (roots, 0) == fixture->source);
    g_list_free (roots);
}

static void
test_get_predecessors (Fixture *fixture, gconstpointer data)
{
    GList *predecessors;

    predecessors = ufo_graph_get_predecessors (fixture->graph, fixture->source);
    g_assert (predecessors == NULL);

    predecessors = ufo_graph_get_predecessors (fixture->graph, fixture->sink1);
    g_assert (g_list_length (predecessors) == 1);
    g_assert (g_list_nth_data (predecessors, 0) == fixture->source);
    g_list_free (predecessors);
}

static void
test_get_successors (Fixture *fixture, gconstpointer data)
{
    GList *successors;

    successors = ufo_graph_get_sucessors (fixture->graph, fixture->sink1);
    g_assert (successors == NULL);

    successors = ufo_graph_get_sucessors (fixture->graph, fixture->source);
    g_assert (g_list_length (successors) == 2);
    g_list_free (successors);
}

static void
test_get_siblings (Fixture *fixture, gconstpointer data)
{
    GList     *siblings;

    siblings = ufo_graph_get_siblings (fixture->graph, fixture->source);
    g_assert (siblings == NULL);

    siblings = ufo_graph_get_siblings (fixture->graph, fixture->sink1);
    g_assert (g_list_length (siblings) == 1);
    g_assert (g_list_nth_data (siblings, 0) == fixture->sink2);
    g_list_free (siblings);
}

void
test_add_graph (void)
{
    g_test_add ("/graph/json/invalid-json",
                Fixture,
                NULL,
                fixture_json_setup,
                test_json_invalid_json,
                fixture_json_teardown);

    g_test_add ("/graph/json/file-not-found",
                Fixture,
                NULL,
                fixture_json_setup,
                test_json_file_not_found,
                fixture_json_teardown);

    g_test_add ("/graph/json/unknown-filter",
                Fixture,
                NULL,
                fixture_json_setup,
                test_json_read_unknown_filter,
                fixture_json_teardown);

    g_test_add ("/graph/json/key-not-found",
                Fixture,
                NULL,
                fixture_json_setup,
                test_json_key_not_found,
                fixture_json_teardown);

    g_test_add ("/graph/get/filters",
                Fixture,
                NULL,
                fixture_filter_setup,
                test_get_filters,
                fixture_filter_teardown);

    g_test_add ("/graph/get/roots",
                Fixture,
                NULL,
                fixture_filter_setup,
                test_get_roots,
                fixture_filter_teardown);

    g_test_add ("/graph/get/predecessors",
                Fixture,
                NULL,
                fixture_filter_setup,
                test_get_predecessors,
                fixture_filter_teardown);

    g_test_add ("/graph/get/successors",
                Fixture,
                NULL,
                fixture_filter_setup,
                test_get_successors,
                fixture_filter_teardown);

    g_test_add ("/graph/get/siblings",
                Fixture,
                NULL,
                fixture_filter_setup,
                test_get_siblings,
                fixture_filter_teardown);
}
