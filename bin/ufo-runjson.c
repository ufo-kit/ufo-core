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

#define _POSIX_C_SOURCE     1

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ufo/ufo.h>

#ifdef WITH_MPI
#include <mpi.h>
#include <ufo/ufo-mpi-messenger.h>
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

static GValueArray *
string_array_to_value_array (gchar **array)
{
    GValueArray *result = NULL;

    if (array == NULL)
        return NULL;
    
    result = g_value_array_new (0);
    
    for (guint i = 0; array[i] != NULL; i++) {
        GValue *tmp = (GValue *) g_malloc0 (sizeof (GValue));
        g_value_init (tmp, G_TYPE_STRING);
        g_value_set_string (tmp, array[i]);
        result = g_value_array_append (result, tmp);
    }

    return result;
}

static void
progress_update (gpointer user)
{
    static int n = 0;
    g_print ("\33[2K\r%i items processed ...", ++n);
}

static void
execute_json (const gchar *filename,
              gchar **addresses)
{
    UfoTaskGraph *task_graph;
    UfoBaseScheduler *scheduler;
    UfoPluginManager *manager;
    GList *leaves;
    UfoResources *resources = NULL;
    GValueArray *address_list = NULL;
    GError *error = NULL;
    gboolean has_tty;

    manager = ufo_plugin_manager_new ();

    task_graph = UFO_TASK_GRAPH (ufo_task_graph_new ());
    ufo_task_graph_read_from_file (task_graph, manager, filename, &error);
    handle_error ("Reading JSON", error, UFO_GRAPH (task_graph));

    has_tty = isatty (fileno (stdin));
    leaves = ufo_graph_get_leaves (UFO_GRAPH (task_graph));

    if (has_tty) {
        UfoTaskNode *leaf;

        leaf = UFO_TASK_NODE (leaves->data);
        g_signal_connect (leaf, "processed", G_CALLBACK (progress_update), NULL);
    }

    scheduler = ufo_scheduler_new ();
    address_list = string_array_to_value_array (addresses);

    if (address_list) {
        resources = UFO_RESOURCES (ufo_resources_new (NULL));
        g_object_set (G_OBJECT (resources), "remotes", address_list, NULL);
        g_value_array_free (address_list);
        ufo_base_scheduler_set_resources (scheduler, resources);
    }

    ufo_base_scheduler_run (scheduler, task_graph, &error);
    handle_error ("Executing", error, UFO_GRAPH (task_graph));

    if (has_tty)
        g_print ("\n");

    g_list_free (leaves);

    g_object_unref (task_graph);
    g_object_unref (scheduler);
    g_object_unref (manager);

    if (resources != NULL)
        g_object_unref (resources);
}

#ifdef WITH_MPI

static void
mpi_terminate_processes (gint global_size)
{
    for (int i = 1; i < global_size; i++) {
        gchar *addr = g_strdup_printf ("%d", i);
        UfoMessage *poisonpill = ufo_message_new (UFO_MESSAGE_TERMINATE, 0);
        UfoMessenger *msger = UFO_MESSENGER (ufo_mpi_messenger_new ());
        ufo_mpi_messenger_connect (msger, addr, UFO_MESSENGER_CLIENT);
        g_debug ("sending poisonpill to %s", addr);
        ufo_messenger_send_blocking (msger, poisonpill, NULL);
        ufo_message_free (poisonpill);
        ufo_messenger_disconnect (msger);
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
    MPI_Init_thread (argc, &argv, MPI_THREAD_MULTIPLE, &provided);

    MPI_Comm_rank (MPI_COMM_WORLD, rank);
    MPI_Comm_size (MPI_COMM_WORLD, global_size);

    if (*global_size == 1) {
        g_critical ("Warning: running MPI instance but found only single process");
        exit (0);
    }

#ifdef DEBUG
    // get us some time to attach a gdb session to the pids
    g_debug ("Process PID %d ranked %d of %d  - ready for attach\n",
             getpid(), rank, *global_size - 1);

    sleep (3);
#endif

}

#endif

int main(int argc, char *argv[])
{
    GOptionContext *context;
    GError *error = NULL;
    gchar **addresses = NULL;
    gboolean show_version = FALSE;

    GOptionEntry entries[] = {
#ifndef WITH_MPI
        { "address", 'a', 0, G_OPTION_ARG_STRING_ARRAY, &addresses,
          "Address of remote server running `ufod'", NULL },
#endif
        { "version", 'v', 0, G_OPTION_ARG_NONE, &show_version,
          "Show version information", NULL },
        { NULL }
    };

#if !(GLIB_CHECK_VERSION (2, 36, 0))
    g_type_init();
#endif

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

#ifdef WITH_MPI
    gint rank, size;
    mpi_init (&argc, argv, &rank, &size);

    if (rank == 0) {
        addresses = mpi_build_addresses (size);
    }
    else {
        gchar *addr = g_strdup_printf("%d", rank);
        UfoDaemon *daemon = ufo_daemon_new (addr);
        ufo_daemon_start (daemon);
        ufo_daemon_wait_finish (daemon);
        MPI_Finalize ();
        exit(EXIT_SUCCESS);
    }
#endif

    execute_json (argv[argc-1], addresses);

#ifdef WITH_MPI
    if (rank == 0) {
        mpi_terminate_processes (size);
        MPI_Finalize ();
    }
#endif

    g_strfreev (addresses);
    g_option_context_free (context);

    return 0;
}
