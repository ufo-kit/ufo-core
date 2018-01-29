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

typedef struct {
    gchar *scheduler;
    gboolean trace;
    gboolean timestamps;
    gboolean version;
    gboolean quiet;
    gboolean quieter;
} Options;

static void
handle_error (const gchar *prefix, GError *error, UfoGraph *graph)
{
    if (error) {
        g_printerr ("%s: %s", prefix, error->message);
        g_error_free (error);
        exit (EXIT_FAILURE);
    }
}

static void
progress_update (gpointer user)
{
    static int n = 0;
    g_print ("\33[2K\r%i items processed ...", ++n);
}

static void
execute_json (const gchar *filename,
              const Options *options)
{
    UfoTaskGraph *task_graph;
    UfoBaseScheduler *scheduler = NULL;
    UfoPluginManager *manager;
    GList *leaves;
    UfoResources *resources = NULL;
    GError *error = NULL;
    gboolean have_tty;

    manager = ufo_plugin_manager_new ();

    task_graph = UFO_TASK_GRAPH (ufo_task_graph_new ());
    ufo_task_graph_read_from_file (task_graph, manager, filename, &error);
    handle_error ("Reading JSON", error, UFO_GRAPH (task_graph));

    have_tty = isatty (fileno (stdin));
    leaves = ufo_graph_get_leaves (UFO_GRAPH (task_graph));

    if (!options->quiet && have_tty) {
        UfoTaskNode *leaf;

        leaf = UFO_TASK_NODE (leaves->data);
        g_signal_connect (leaf, "processed", G_CALLBACK (progress_update), NULL);
    }

    if ((NULL != options->scheduler) && (0 == g_ascii_strcasecmp (options->scheduler, "fixed"))) {
        g_debug ("INFO: run-json: using fixed-scheduler");
        scheduler = ufo_fixed_scheduler_new ();
    }

    /*
    if ((NULL != options->scheduler) && (0 == g_ascii_strcasecmp (options->scheduler, "local"))) {
        g_debug ("INFO: run-json: using local-scheduler");
        scheduler = ufo_local_scheduler_new ();
    }

    if ((NULL != options->scheduler) && (0 == g_ascii_strcasecmp (options->scheduler, "group"))) {
        g_debug ("INFO: run-json: using group-scheduler");
        scheduler = ufo_group_scheduler_new ();
    }
    */

    if ((NULL != options->scheduler) && (0 == g_ascii_strcasecmp (options->scheduler, "dynamic"))) {
        g_debug ("INFO: run-json: using dynamic scheduler");
        scheduler = ufo_scheduler_new ();
    }

    if (!scheduler) {
        g_debug ("INFO: run-json: using dynamic scheduler by default");
        scheduler = ufo_scheduler_new ();
    }

    g_object_set (scheduler,
                  "enable-tracing", options->trace,
                  "timestamps", options->timestamps,
                  NULL);

    ufo_base_scheduler_run (scheduler, task_graph, &error);
    handle_error ("Executing", error, UFO_GRAPH (task_graph));

    if (!options->quieter) {
        gdouble run_time;

        if (!options->quiet && have_tty)
            g_print ("\n");

        g_object_get (scheduler, "time", &run_time, NULL);
        g_print ("Finished in %3.5fs\n", run_time);
    }

    g_list_free (leaves);

    g_object_unref (task_graph);
    g_object_unref (scheduler);
    g_object_unref (manager);

    if (resources != NULL)
        g_object_unref (resources);
}

int main(int argc, char *argv[])
{
    GOptionContext *context;
    GError *error = NULL;

    static Options options = {
        .scheduler = NULL,
        .trace = FALSE,
        .timestamps = FALSE,
        .version = FALSE,
        .quiet = FALSE,
        .quieter = FALSE,
    };

    GOptionEntry entries[] = {
        { "trace",     't', 0, G_OPTION_ARG_NONE, &options.trace, "enable tracing", NULL },
        { "scheduler", 's', 0, G_OPTION_ARG_STRING, &options.scheduler, "selecting a scheduler",
          "dynamic|fixed"},
        { "timestamps",  0, 0, G_OPTION_ARG_NONE, &options.timestamps, "enable timestamps", NULL },
        { "quiet",     'q', 0, G_OPTION_ARG_NONE, &options.quiet, "be quiet", NULL },
        { "quieter",     0, 0, G_OPTION_ARG_NONE, &options.quieter, "be quieter", NULL },
        { "version",   'v', 0, G_OPTION_ARG_NONE, &options.version, "Show version information", NULL },
        { NULL }
    };

#if !(GLIB_CHECK_VERSION (2, 36, 0))
    g_type_init();
#endif

    context = g_option_context_new ("FILE");
    g_option_context_add_main_entries (context, entries, NULL);

    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_printerr ("Option parsing failed: %s\n", error->message);
        return 1;
    }

    if (options.version) {
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

    execute_json (argv[argc-1], &options);

    g_option_context_free (context);

    return 0;
}
