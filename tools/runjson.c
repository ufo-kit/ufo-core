
#include <stdlib.h>
#include <glib-object.h>
#include <ufo-config.h>
#include <ufo-task-graph.h>
#include <ufo-arch-graph.h>
#include <ufo-scheduler.h>
#include <ufo-resources.h>

static void
handle_error (const gchar *prefix, GError *error, UfoGraph *graph)
{
    if (error) {
        g_warning ("%s: %s", prefix, error->message);
        g_error_free (error);
        exit (EXIT_FAILURE);
    }
}

static GList *
string_array_to_list (gchar **array)
{
    GList *result = NULL;

    if (array == NULL)
        return NULL;

    for (guint i = 0; array[i] != NULL; i++)
        result = g_list_append (result, array[i]);

    return result;
}

static void
execute_json (const gchar *filename,
              gchar **paths,
              gchar **addresses)
{
    UfoConfig       *config;
    UfoArchGraph    *arch_graph;
    UfoTaskGraph    *task_graph;
    UfoScheduler    *scheduler;
    UfoResources    *resources;
    UfoPluginManager *manager;
    GList *path_list = NULL;
    GList *address_list = NULL;
    GError *error = NULL;

    config = ufo_config_new ();

    path_list = string_array_to_list (paths);
    ufo_config_add_paths (config, path_list);
    g_list_free (path_list);

    manager = ufo_plugin_manager_new (config);

    task_graph = UFO_TASK_GRAPH (ufo_task_graph_new ());
    ufo_task_graph_read_from_file (task_graph, manager, filename, &error);
    handle_error ("Reading JSON", error, UFO_GRAPH (task_graph));

    address_list = string_array_to_list (addresses);
    resources = ufo_resources_new (config);
    arch_graph = UFO_ARCH_GRAPH (ufo_arch_graph_new (address_list, resources));
    g_list_free (address_list);

    scheduler = ufo_scheduler_new ();
    ufo_scheduler_run (scheduler, arch_graph, task_graph, &error);
    handle_error ("Executing", error, UFO_GRAPH (task_graph));

    g_object_unref (arch_graph);
    g_object_unref (task_graph);
    g_object_unref (scheduler);
    g_object_unref (manager);
    g_object_unref (resources);
    g_object_unref (config);
}

int main(int argc, char *argv[])
{
    GOptionContext *context;
    GError *error = NULL;
    gchar **paths = NULL;
    gchar **addresses = NULL;

    GOptionEntry entries[] = {
        { "path", 'p', 0, G_OPTION_ARG_STRING_ARRAY, &paths, "Path to node plugins or OpenCL kernels", NULL },
        { "address", 'a', 0, G_OPTION_ARG_STRING_ARRAY, &addresses, "Address of remote server running `ufod'", NULL },
        { NULL }
    };

    g_type_init();

    context = g_option_context_new ("FILE");
    g_option_context_add_main_entries (context, entries, NULL);

    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_print ("Option parsing failed: %s\n", error->message);
        return 1;
    }

    if (argc < 2) {
        gchar *help;

        help = g_option_context_get_help (context, TRUE, NULL);
        g_print ("%s", help);
        g_free (help);
        return 1;
    }

    execute_json (argv[argc-1], paths, addresses);

    g_strfreev (paths);
    g_strfreev (addresses);
    g_option_context_free (context);

    return 0;
}
