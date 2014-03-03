/*
 * Copyright (C) 2011-2013 Karlsruhe Institute of Technology
 *
 * This file is part of Ufo.
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib-object.h>
#include "test-suite.h"
#ifdef MPI
#include <mpi.h>
#endif

static void
ignore_log (const gchar     *domain,
            GLogLevelFlags   flags,
            const gchar     *message,
            gpointer         data)
{
    g_print ("%s\n",message);
}

int main(int argc, char *argv[])
{
    g_type_init ();
    g_test_init (&argc, &argv, NULL);
    g_test_bug_base ("https://github.com/ufo-kit/ufo-core/issues");

    g_log_set_handler ("Ufo", G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG, ignore_log, NULL);
    g_log_set_handler ("ocl", G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG, ignore_log, NULL);
    g_log_set_fatal_mask ("Ufo", 0);
    test_add_buffer ();
    g_test_run();
    return 0;
    test_add_remote_node ();
    test_add_config ();
    test_add_graph ();
    test_add_profiler ();

#ifdef MPI
    int provided;
    MPI_Init_thread (&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    test_add_mpi_remote_node ();
#else
    test_add_zmq_messenger ();
    test_add_remote_node ();
#endif

    g_test_run();

#ifdef MPI
    MPI_Finalize ();
#endif

    return 0;
}
