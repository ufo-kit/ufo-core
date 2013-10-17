/*
 * Copyright (C) 2011-2013 Karlsruhe Institute of Technology
 *
 * This file is part of runjson.
 *
 * runjson is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * runjson is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with runjson.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <stdlib.h>
#include <ufo/ufo.h>

#ifdef MPI
#include <mpi.h>
#include <unistd.h>
#endif

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

static UfoConfig *
get_config (gchar **paths)
{
    GList *path_list = NULL;
    UfoConfig *config;

    config = ufo_config_new ();
    path_list = string_array_to_list (paths);
    ufo_config_add_paths (config, path_list);

    g_list_free (path_list);
    return config;
}

static void
execute_json (const gchar *filename,
              UfoConfig *config,
              gchar **addresses)
{
    UfoTaskGraph    *task_graph;
    UfoScheduler    *scheduler;
    UfoPluginManager *manager;
    GList *address_list = NULL;
    GError *error = NULL;

    manager = ufo_plugin_manager_new (config);

    task_graph = UFO_TASK_GRAPH (ufo_task_graph_new ());
    ufo_task_graph_read_from_file (task_graph, manager, filename, &error);
    handle_error ("Reading JSON", error, UFO_GRAPH (task_graph));

    address_list = string_array_to_list (addresses);
    scheduler = ufo_scheduler_new (config, address_list);
    g_list_free (address_list);

    ufo_scheduler_run (scheduler, task_graph, &error);
    handle_error ("Executing", error, UFO_GRAPH (task_graph));

    g_object_unref (task_graph);
    g_object_unref (scheduler);
    g_object_unref (manager);
}

int main(int argc, char *argv[])
{
    GOptionContext *context;
    GError *error = NULL;
    gchar **paths = NULL;
    gchar **addresses = NULL;
    gboolean show_version = FALSE;
    UfoConfig *config = NULL;

    GOptionEntry entries[] = {
        { "path", 'p', 0, G_OPTION_ARG_STRING_ARRAY, &paths,
          "Path to node plugins or OpenCL kernels", NULL },
#ifndef MPI
        { "address", 'a', 0, G_OPTION_ARG_STRING_ARRAY, &addresses,
          "Address of remote server running `ufod'", NULL },
#endif
        { "version", 'v', 0, G_OPTION_ARG_NONE, &show_version,
          "Show version information", NULL },
        { NULL }
    };

    g_type_init();

    context = g_option_context_new ("FILE");
    g_option_context_add_main_entries (context, entries, NULL);

    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_print ("Option parsing failed: %s\n", error->message);
        return 1;
    }

    if (show_version) {
        g_print ("runjson %s\n", UFO_VERSION);
        exit (EXIT_SUCCESS);
    }

    if (argc < 2) {
        gchar *help;

        help = g_option_context_get_help (context, TRUE, NULL);
        g_print ("%s", help);
        g_free (help);
        return 1;
    }

    config = get_config (paths);

#ifdef MPI
    int rank, size;
    MPI_Init (&argc, &argv);
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
    MPI_Comm_size (MPI_COMM_WORLD, &size);

    g_debug ("Process PID %d ranked %d of %d  - ready for attach\n",
             getpid(), rank, size - 1);

    if (rank > 0) {
        gchar *addr = g_strdup_printf("%d", rank);
        UfoDaemon *daemon = ufo_daemon_new (config, addr);
        ufo_daemon_start (daemon);
        while (TRUE) {
            g_usleep (G_MAXULONG);
        }
        exit(EXIT_SUCCESS);
    }

    // build addresses by MPI_COMM_WORLD size, exclude rank 0
    addresses = g_malloc (sizeof (char*) * size);
    for (int i = 1; i < size; i++) {
        addresses[i-1] = g_strdup_printf ("%d", i);
    }
    addresses[size-1] = NULL;
#endif

    execute_json (argv[argc-1], config, addresses);

    g_strfreev (paths);
    g_strfreev (addresses);
    g_option_context_free (context);
    g_object_unref (config);

    return 0;
}
