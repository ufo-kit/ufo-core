
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
        g_error ("%s: %s", prefix, error->message);
        g_error_free (error);
        g_object_unref (graph);
        exit (EXIT_FAILURE);
    }
}

static GValueArray *
make_path_array (gchar **paths)
{
    GValueArray *array;
    GValue  value = {0};

    array = g_value_array_new (2);
    g_value_init (&value, G_TYPE_STRING);

    for (guint i = 0; paths[i] != NULL; i++) {
        g_value_reset (&value);
        g_value_set_string (&value, paths[i]);
        g_value_array_append (array, &value);
    }

    return array;
}

static void
execute_json (const gchar *filename,
              GValueArray *paths)
{
    UfoConfig       *config;
    UfoArchGraph    *arch_graph;
    UfoTaskGraph    *task_graph;
    UfoScheduler    *scheduler;
    UfoResources    *resources;
    UfoPluginManager *manager;
    GError *error = NULL;

    config = ufo_config_new ();

    if (paths != NULL)
        g_object_set (G_OBJECT (config), "paths", paths, NULL);

    manager = ufo_plugin_manager_new (config);

    task_graph = UFO_TASK_GRAPH (ufo_task_graph_new ());
    ufo_task_graph_read_from_file (task_graph, manager, filename, &error);
    handle_error ("Reading JSON", error, UFO_GRAPH (task_graph));

    resources = ufo_resources_new (config);
    arch_graph = UFO_ARCH_GRAPH (ufo_arch_graph_new (NULL, resources));
    scheduler = ufo_scheduler_new ();

    ufo_scheduler_set_task_split (scheduler, FALSE);
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
    GValueArray *path_array = NULL;
    gchar **paths = NULL;

    GOptionEntry entries[] = {
        { "path", 'p', 0, G_OPTION_ARG_STRING_ARRAY, &paths, "Path to node plugins or OpenCL kernels", NULL },
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

    if (paths != NULL) {
        path_array = make_path_array (paths);
        g_strfreev (paths);
    }

    execute_json (argv[argc-1], path_array);

    g_value_array_free (path_array);
    g_option_context_free (context);

    return 0;
}
