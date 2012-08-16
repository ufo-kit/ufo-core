#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
#include "test-suite.h"
#include "ufo-graph.h"
#include "ufo-plugin-manager.h"

typedef struct {
    UfoGraph         *graph;
    UfoPluginManager *manager;
    gchar            *valid_json_file;
    gchar            *invalid_json_file;
    gchar            *empty_json_file;
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
fixture_setup (Fixture *fixture, gconstpointer data)
{
    static const gchar *valid_json =
        "{\
            \"nodes\": [{\"plugin\": \"foobarSie8Shoh\", \"name\": \"foobar\"}],\
            \"edges\": []\
        }";
    static const gchar *invalid_json = "{\"nodes:[]}"; /* missing " */
    static const gchar *empty_json = "{\"nodes\": [{}]}";

    fixture->graph = ufo_graph_new ("");
    g_assert (UFO_IS_GRAPH (fixture->graph));

    fixture->manager = ufo_plugin_manager_new ("");
    g_assert (UFO_IS_PLUGIN_MANAGER (fixture->manager));

    fixture->valid_json_file = g_strdup_printf ("valid.json");
    fixture->invalid_json_file = g_strdup_printf ("invalid.json");
    fixture->empty_json_file = g_strdup_printf ("empty.json");

    write_file (fixture->valid_json_file, valid_json);
    write_file (fixture->invalid_json_file, invalid_json);
    write_file (fixture->empty_json_file, empty_json);
}

static void
fixture_teardown (Fixture *fixture, gconstpointer data)
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

void
test_add_graph (void)
{
    g_test_add ("/graph/json/invalid-json",
                Fixture,
                NULL,
                fixture_setup,
                test_json_invalid_json,
                fixture_teardown);

    g_test_add ("/graph/json/file-not-found",
                Fixture,
                NULL,
                fixture_setup,
                test_json_file_not_found,
                fixture_teardown);

    g_test_add ("/graph/json/unknown-filter",
                Fixture,
                NULL,
                fixture_setup,
                test_json_read_unknown_filter,
                fixture_teardown);

    g_test_add ("/graph/json/key-not-found",
                Fixture,
                NULL,
                fixture_setup,
                test_json_key_not_found,
                fixture_teardown);
}
