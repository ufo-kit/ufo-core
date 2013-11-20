/*
 * Copyright (C) 2011-2013 Karlsruhe Institute of Technology
 *
 * This file is part of ufod.
 *
 * ufod is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * ufod is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with ufod.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <ufo/ufo.h>

static UfoDaemon *daemon;

typedef struct {
    gchar **paths;
    gchar *addr;
    gboolean debug;
} Options;

static Options *
opts_parse (gint *argc, gchar ***argv)
{
    GOptionContext *context;
    Options *opts;
    gboolean show_version = FALSE;
    gboolean debug = FALSE;
    GError *error = NULL;

    opts = g_new0 (Options, 1);

    GOptionEntry entries[] = {
        { "listen", 'l', 0, G_OPTION_ARG_STRING, &opts->addr,
          "Address to listen on (see http://api.zeromq.org/3-2:zmq-tcp)", NULL },
        { "path", 'p', 0, G_OPTION_ARG_STRING_ARRAY, &opts->paths,
          "Path to node plugins or OpenCL kernels", NULL },
        { "version", 'v', 0, G_OPTION_ARG_NONE, &show_version,
          "Show version information", NULL },
        { "debug", 'd', 0, G_OPTION_ARG_NONE, &debug,
          "Enable debug messages", NULL },
        { NULL }
    };

    context = g_option_context_new ("FILE");
    g_option_context_add_main_entries (context, entries, NULL);

    if (!g_option_context_parse (context, argc, argv, &error)) {
        g_print ("Option parsing failed: %s\n", error->message);
        g_free (opts);
        return NULL;
    }

    if (show_version) {
        g_print ("ufod %s\n", UFO_VERSION);
        exit (EXIT_SUCCESS);
    }

    if (opts->addr == NULL)
        opts->addr = g_strdup ("tcp://*:5555");

    opts->debug = debug;

    g_option_context_free (context);

    return opts;
}

static UfoConfig *
opts_new_config (const Options *opts)
{
    UfoConfig *config;
    GList *paths = NULL;

    config = ufo_config_new ();

    if (opts->paths != NULL) {
        for (guint i = 0; opts->paths[i] != NULL; i++)
            paths = g_list_append (paths, g_strdup (opts->paths[i]));

        ufo_config_add_paths (config, paths);
        g_list_free (paths);
    }

    return config;
}

static void
opts_free (Options *opts)
{
    g_strfreev (opts->paths);
    g_free (opts->addr);
    g_free (opts);
}

static void
terminate (int signum)
{
    if (signum == SIGTERM)
        g_print ("Received SIGTERM, exiting...\n");
    if (signum == SIGINT)
        g_print ("Received SIGINT, exiting...\n");

    if (daemon != NULL) {
        ufo_daemon_stop(daemon);
        g_object_unref (daemon);
    }
    exit (EXIT_SUCCESS);
}

static void
log_handler (const gchar     *domain,
             GLogLevelFlags   flags,
             const gchar     *message,
             gpointer         data)
{
    g_print ("%s\n",message);
}

int
main (int argc, char * argv[])
{
    Options *opts;

    g_type_init ();
    g_thread_init (NULL);

    if ((opts = opts_parse (&argc, &argv)) == NULL)
        return 1;

    if (opts->debug)
        g_log_set_handler ("Ufo", G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG, log_handler, NULL);

    /* Now, install SIGTERM/SIGINT handler */
    (void) signal (SIGTERM, terminate);
    (void) signal (SIGINT, terminate);

    UfoConfig *config = opts_new_config (opts);

    daemon = ufo_daemon_new (config, opts->addr);
    ufo_daemon_start(daemon);
    g_print ("ufod %s - waiting for requests on %s ...\n", UFO_VERSION,
                                                           opts->addr);

    while (TRUE) {
        g_usleep (G_MAXULONG);
    }

    opts_free (opts);

    return 0;
}
