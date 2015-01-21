/*
 * Copyright (C) 2011-2014 Karlsruhe Institute of Technology
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

#include <stdlib.h>
#include <ufo/ufo.h>
#include "ufo/compat.h"


typedef struct {
    char *name;
    GList *props;
} TaskDescription;


#define DEFINE_CAST(suffix, trans_func)                 \
static void                                             \
value_transform_##suffix (const GValue *src_value,      \
                         GValue       *dest_value)      \
{                                                       \
  const gchar* src = g_value_get_string (src_value);    \
  g_value_set_##suffix (dest_value, trans_func (src));  \
}

DEFINE_CAST (schar,     atoi)
DEFINE_CAST (uchar,     atoi)
DEFINE_CAST (int,       atoi)
DEFINE_CAST (long,      atol)
DEFINE_CAST (uint,      atoi)
DEFINE_CAST (ulong,     atol)
DEFINE_CAST (float,     atof)
DEFINE_CAST (double,    atof)


static GList *
tokenize_args (int n, char* argv[])
{
    enum {
        NEW_TASK,
        NEW_PROP,
    } state = NEW_TASK;
    GList *tasks = NULL;
    TaskDescription *current;

    for (int i = 0; i < n; i++) {
        if (state == NEW_TASK) {
            current = g_new0 (TaskDescription, 1);
            current->name = argv[i];
            tasks = g_list_append (tasks, current);
            state = NEW_PROP;
        }
        else {
            if (!g_strcmp0 (g_strstrip (argv[i]), "!"))
                state = NEW_TASK;
            else
                current->props = g_list_append (current->props, argv[i]);
        }
    }

    return tasks;
}

static UfoTaskGraph *
parse_pipeline (GList *pipeline, UfoPluginManager *pm, GError **error)
{
    GList *it;
    GRegex *assignment;
    UfoTaskGraph *graph;
    UfoTaskNode *prev = NULL;

    assignment = g_regex_new ("\\s*([A-Za-z0-9]*)=(.*)\\s*", 0, 0, error);

    if (*error)
        return NULL;

    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_CHAR,    value_transform_schar);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_UCHAR,   value_transform_uchar);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_INT,     value_transform_int);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_UINT,    value_transform_uint);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_LONG,    value_transform_long);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_ULONG,   value_transform_ulong);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_FLOAT,   value_transform_float);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_DOUBLE,  value_transform_double);

    graph = UFO_TASK_GRAPH (ufo_task_graph_new ());

    g_list_for (pipeline, it) {
        GList *jt;
        GMatchInfo *match;
        TaskDescription *desc;
        UfoTaskNode *task;

        desc = (TaskDescription *) (it->data);
        task = ufo_plugin_manager_get_task (pm, desc->name, error);

        if (task == NULL)
            return NULL;

        g_list_for (desc->props, jt) {
            gchar *prop_assignment;

            prop_assignment = (gchar *) jt->data;
            g_regex_match (assignment, prop_assignment, 0, &match);

            if (g_match_info_matches (match)) {
                GParamSpec *pspec;
                GValue value = G_VALUE_INIT;

                gchar *prop = g_match_info_fetch (match, 1);
                gchar *string_value = g_match_info_fetch (match, 2);

                g_value_init (&value, G_TYPE_STRING);
                g_value_set_string (&value, string_value);

                pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (task), prop);

                if (pspec != NULL) {
                    GValue target_value = G_VALUE_INIT;

                    g_value_init (&target_value, pspec->value_type);
                    g_value_transform (&value, &target_value);
                    g_object_set_property (G_OBJECT (task), prop, &target_value);
                }
                else {
                    g_warning ("`%s' does not have property `%s'", desc->name, prop);
                }

                g_match_info_free (match);
                g_free (prop);
                g_free (string_value);
            }
            else {
                g_warning ("Expected property assignment or `!' but got `%s' instead", prop_assignment);
            }
        }

        if (prev != NULL)
            ufo_task_graph_connect_nodes (graph, prev, task);

        prev = task;
    }

    g_regex_unref (assignment);
    return graph;
}

int
main(int argc, char* argv[])
{
    UfoPluginManager *pm;
    UfoTaskGraph *graph;
    UfoBaseScheduler *sched;
    GList *pipeline;
    GError *error = NULL;

#if !(GLIB_CHECK_VERSION (2, 36, 0))
    g_type_init ();
#endif

    if (argc == 1)
        return 0;

    pipeline = tokenize_args (argc - 1, &argv[1]);
    pm = ufo_plugin_manager_new ();
    graph = parse_pipeline (pipeline, pm, &error);

    if (error != NULL) {
        g_print ("Error parsing pipeline: %s\n", error->message);
        return 1;
    }

    sched = ufo_scheduler_new ();
    ufo_base_scheduler_run (sched, graph, &error);

    if (error != NULL) {
        g_print ("Error executing pipeline: %s\n", error->message);
    }

    g_object_unref (graph);
    g_object_unref (pm);

    return 0;
}
