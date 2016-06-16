/*
 * Copyright (C) 2011-2015 Karlsruhe Institute of Technology
 *
 * This file is part of ufo-launch.
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

#include <ufo/ufo.h>
#include "ufo/compat.h"


static gboolean list = FALSE;
static gchar *prop_name = NULL;
static gboolean verbose = FALSE;
static gboolean version = FALSE;


static void
list_tasks (UfoPluginManager *pm)
{
    GList *names;
    GList *it;

    names = ufo_plugin_manager_get_all_task_names (pm);

    g_list_for (names, it) {
        g_print ("%s\n", (const gchar *) it->data);
    }

    g_list_free (names);
    g_object_unref (pm);
}

static void
list_properties (UfoPluginManager *pm, const gchar *name)
{
    UfoTaskNode *task;
    guint n_props;
    GParamSpec **props;
    GError *error = NULL;

    task = ufo_plugin_manager_get_task (pm, name, &error);

    if (error != NULL) {
        g_printerr ("Error: %s\n", error->message);
        return;
    }

    props = g_object_class_list_properties (G_OBJECT_GET_CLASS (task), &n_props);

    for (guint i = 0; i < n_props; i++) {
        if (g_strcmp0 (props[i]->name, "num-processed")) {
            g_print ("%s\n", g_param_spec_get_name (props[i]));

            if (verbose) {
                g_print ("  type: %s\n", g_type_name (props[i]->value_type));
                g_print ("  help: %s\n", g_param_spec_get_blurb (props[i]));

                if (i < n_props - 1)
                    g_print ("\n");
            }
        }
    }

    g_free (props);
    g_object_unref (task);
}

int
main(int argc, char* argv[])
{
    UfoPluginManager *pm;
    GOptionContext *context;
    GError *error = NULL;

    static GOptionEntry entries[] = {
        { "list",    'l', 0, G_OPTION_ARG_NONE, &list, "list available tasks", NULL },
        { "props",   'p', 0, G_OPTION_ARG_STRING, &prop_name, "Properties of given task", NULL },
        { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL },
        { "version",   0, 0, G_OPTION_ARG_NONE, &version, "Show version information", NULL },
        { NULL }
    };

#if !(GLIB_CHECK_VERSION (2, 36, 0))
    g_type_init ();
#endif

    context = g_option_context_new (NULL);
    g_option_context_add_main_entries (context, entries, NULL);

    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_printerr ("Error parsing options: %s\n", error->message);
        return 1;
    }

    if (version) {
        g_print ("%s version " UFO_VERSION "\n", argv[0]);
        return 0;
    }


    pm = ufo_plugin_manager_new ();

    if (list)
        list_tasks (pm);

    if (prop_name)
        list_properties (pm, prop_name);

    return 0;
}
