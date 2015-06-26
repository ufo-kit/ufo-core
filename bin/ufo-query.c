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

/* #include <stdlib.h> */
#include <ufo/ufo.h>
#include "ufo/compat.h"

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
        g_print ("Error: %s\n", error->message);
        return;
    }

    props = g_object_class_list_properties (G_OBJECT_GET_CLASS (task), &n_props);

    for (guint i = 0; i < n_props; i++) {
        if (g_strcmp0 (props[i]->name, "num-processed"))
            g_print ("%s\n", props[i]->name);
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

    static gboolean list = FALSE;
    static gchar *props = NULL;

    static GOptionEntry entries[] = {
        { "list", 'l', 0, G_OPTION_ARG_NONE, &list, "list available tasks", NULL },
        { "props", 'p', 0, G_OPTION_ARG_STRING, &props, "Properties of given task", NULL },
        { NULL }
    };

#if !(GLIB_CHECK_VERSION (2, 36, 0))
    g_type_init ();
#endif

    context = g_option_context_new (NULL);
    g_option_context_add_main_entries (context, entries, NULL);

    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_print ("Error parsing options: %s\n", error->message);
        return 1;
    }

    pm = ufo_plugin_manager_new ();

    if (list)
        list_tasks (pm);

    if (props)
        list_properties (pm, props);

    return 0;
}
