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
#include <ufo/ufo-mpi-messenger.h>
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

#ifdef MPI
static GStaticMutex static_mutex = G_STATIC_MUTEX_INIT;

static void
mpi_terminate_processes (gint global_size)
{
    GMutex *mutex = g_static_mutex_get_mutex (&static_mutex);
    for (int i = 1; i < global_size; i++) {
        g_mutex_lock (mutex);
        gchar *addr = g_strdup_printf ("%d", i);
        UfoMessage *poisonpill = ufo_message_new (UFO_MESSAGE_TERMINATE, 0);
        UfoMessenger *msger = UFO_MESSENGER (ufo_mpi_messenger_new ());
        ufo_mpi_messenger_connect (msger, addr, UFO_MESSENGER_CLIENT);
        g_debug ("sending poisonpill to %s", addr);
        ufo_messenger_send_blocking (msger, poisonpill, NULL);
        ufo_message_free (poisonpill);
        ufo_messenger_disconnect (msger);
        g_mutex_unlock (mutex);
    }
}

static gchar**
mpi_build_addresses (gint global_size)
{
    /* build addresses by MPI_COMM_WORLD size, exclude rank 0 but
       have room for NULL termination */
    gchar **addresses = g_malloc (sizeof (gchar *) * global_size);
    for (int i = 1; i < global_size; i++) {
        addresses[i - 1] = g_strdup_printf ("%d", i);
    }
    addresses[global_size - 1] = NULL;

    return addresses;
}

static void
mpi_init (int *argc, char *argv[], gint *rank, gint *global_size)
{
    gint provided;
    MPI_Init_thread (argc, &argv, MPI_THREAD_SERIALIZED, &provided);
    MPI_Comm_rank (MPI_COMM_WORLD, rank);
    MPI_Comm_size (MPI_COMM_WORLD, global_size);

    if (*global_size == 1) {
        g_critical ("Warning: running MPI instance but found only single process");
        exit (0);
    }

#ifdef DEBUG
    // get us some time to attach a gdb session to the pids
    g_debug ("Process PID %d ranked %d of %d  - ready for attach\n",
             getpid(), *rank, *global_size - 1);

    //sleep (5);
#endif

}

#endif

static void
log_handler (const gchar     *domain,
            GLogLevelFlags   flags,
            const gchar     *message,
            gpointer         data)
{
    g_print ("%s\n",message);
}

int main(int argc, char *argv[])
{
    GOptionContext *context;
    GError *error = NULL;
    gchar **paths = NULL;
    gchar **addresses = NULL;
    gboolean network_writer = FALSE;
    gboolean debug = FALSE;
    gboolean show_version = FALSE;
    gboolean disable_gpu = FALSE;
    UfoConfig *config = NULL;

    GOptionEntry entries[] = {
        { "path", 'p', 0, G_OPTION_ARG_STRING_ARRAY, &paths,
          "Path to node plugins or OpenCL kernels", NULL },
#ifndef MPI
        { "address", 'a', 0, G_OPTION_ARG_STRING_ARRAY, &addresses,
          "Address of remote server running `ufod'", NULL },
#endif
        { "disable-gpu", 'n', 0, G_OPTION_ARG_NONE, &disable_gpu,
          "Don't use local system for GPU computations", NULL },
        { "version", 'v', 0, G_OPTION_ARG_NONE, &show_version,
          "Show version information", NULL },
        { "network-writer", 'w', 0, G_OPTION_ARG_NONE, &network_writer,
          "Assign the writer task to last remote node", NULL },
        { "debug", 'd', 0, G_OPTION_ARG_NONE, &debug,
          "Print debug log messages", NULL },
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
    g_debug ("get_config");
    config = get_config (paths);
    g_object_set (G_OBJECT (config), "disable-gpu", disable_gpu, NULL);

    g_object_set (G_OBJECT (config), "disable-gpu", disable_gpu, NULL);
    g_object_set (G_OBJECT (config), "network-writer", network_writer, NULL);
    g_object_set (G_OBJECT (config), "debug", debug, NULL);
    g_debug ("end option set");

    g_log_set_handler ("Ufo", G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG, log_handler, NULL);

#ifdef MPI
    gint rank, size;
    g_debug ("initing mpi");
    mpi_init (&argc, argv, &rank, &size);

    if (rank == 0) {
    	g_object_set (G_OBJECT (config), "disable-gpu", TRUE, NULL);
        addresses = mpi_build_addresses (size);
    } else {
    	g_object_set (G_OBJECT (config), "disable-gpu", FALSE, NULL);
        gchar *addr = g_strdup_printf("%d", rank);
        UfoDaemon *daemon = ufo_daemon_new (config, addr);
        ufo_daemon_start (daemon);
        ufo_daemon_wait_finish (daemon);
        MPI_Finalize ();
        exit(EXIT_SUCCESS);
    }
#endif

    execute_json (argv[argc-1], config, addresses);

#ifdef MPI
    if (rank == 0) {
        mpi_terminate_processes (size);
        MPI_Finalize ();
    }
#endif

    g_strfreev (paths);
    g_strfreev (addresses);
    g_option_context_free (context);
    g_object_unref (config);

    return 0;
}
