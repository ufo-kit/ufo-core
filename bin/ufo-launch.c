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

#define _POSIX_C_SOURCE     1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ufo/ufo.h>
#include "ufo/compat.h"


typedef struct {
    char *name;
    GList *props;
} TaskDescription;


static gboolean
str_to_boolean (const gchar *s)
{
    return g_ascii_strncasecmp (s, "true", 4) == 0;
}

#define DEFINE_CAST(suffix, trans_func)                 \
static void                                             \
value_transform_##suffix (const GValue *src_value,      \
                         GValue       *dest_value)      \
{                                                       \
  const gchar* src = g_value_get_string (src_value);    \
  g_value_set_##suffix (dest_value, trans_func (src));  \
}

DEFINE_CAST (uchar,     atoi)
DEFINE_CAST (int,       atoi)
DEFINE_CAST (long,      atol)
DEFINE_CAST (uint,      atoi)
DEFINE_CAST (uint64,    atoi)
DEFINE_CAST (ulong,     atol)
DEFINE_CAST (float,     atof)
DEFINE_CAST (double,    atof)
DEFINE_CAST (boolean,   str_to_boolean)

typedef enum {
    STRING      = 1,
    SPACE       = 1 << 1,
    ASSIGNMENT  = 1 << 2,
    EXCLAMATION = 1 << 3,
    PAREN_OPEN  = 1 << 4,
    PAREN_CLOSE = 1 << 5,
    COMMA       = 1 << 6
} TokenType;


typedef struct {
    TokenType type;
    GString *str;
    guint pos;
} Token;


static GList *
tokenize_args (const gchar *pipeline)
{
    GList *tokens = NULL;
    Token *current = NULL;
    guint pos = 0;

    struct {
        gchar c;
        gint type;
    } map[] =
    {
        {'=', ASSIGNMENT},
        {'!', EXCLAMATION},
        {',', COMMA},
        {'[', PAREN_OPEN},
        {']', PAREN_CLOSE},
        {' ', SPACE},
        {0, 0}
    };

    for (const gchar *s = pipeline; *s != '\0'; ++s, pos++) {
        guint i = 0;

        for (; map[i].c != 0; i++) {
            if (*s == map[i].c) {
                Token *t = g_new0 (Token, 1);
                t->type = map[i].type;
                t->pos = pos;
                tokens = g_list_append (tokens, t);
                current = NULL;
                break;
            }
        }

        /* we have a string */
        if (map[i].c == 0) {
            if (current == NULL) {
                current = g_new0 (Token, 1);
                current->type = STRING;
                current->str = g_string_new (NULL);
                current->pos = pos;
                tokens = g_list_append (tokens, current);
            }

            g_string_append_c (current->str, *s);
        }
    }

    return tokens;
}


static void
set_property (UfoTaskNode *task, const gchar *key, const gchar *pvalue)
{
    GParamSpec *pspec;
    GValue value = {0};

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_string (&value, pvalue);

    pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (task), key);

    if (pspec != NULL) {
        GValue target_value = {0};

        g_value_init (&target_value, pspec->value_type);
        g_value_transform (&value, &target_value);
        g_object_set_property (G_OBJECT (task), key, &target_value);
    }
    else {
        g_warning ("`%s' does not have property `%s'", G_OBJECT_TYPE_NAME (task), key);
    }
}

static UfoTaskGraph *
parse (const gchar *pipeline, GList *tokens, UfoPluginManager *pm, GError **error)
{
    GList *it;
    GQueue *stack;
    UfoTaskGraph *graph;
    UfoTaskNode *task = NULL;
    gchar *key = NULL;
    GList *group = NULL;

    stack = g_queue_new ();

    typedef enum {
        NEW_TASK    = 1,
        PROP_KEY    = 1 << 1,
        PROP_VALUE  = 1 << 2,
        CONNECT     = 1 << 3
    } Action;

    Action action = NEW_TASK;

    gint expected = PAREN_OPEN | STRING;

    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_UCHAR,   value_transform_uchar);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_INT,     value_transform_int);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_UINT,    value_transform_uint);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_UINT64,  value_transform_uint64);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_LONG,    value_transform_long);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_ULONG,   value_transform_ulong);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_FLOAT,   value_transform_float);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_DOUBLE,  value_transform_double);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_BOOLEAN, value_transform_boolean);

    graph = UFO_TASK_GRAPH (ufo_task_graph_new ());

    /* push default group */
    g_queue_push_head (stack, NULL);

    g_list_for (tokens, it) {
        Token *t = (Token *) it->data;

        if ((expected & t->type) == 0) {
            g_print ("Error: cannot parse pipeline specification\n  %s\n", pipeline);

            for (guint i = 0; i < t->pos + 2; i++)
                g_print (" ");

            g_print ("^\n");
            return NULL;
        }

        switch (t->type) {
            case PAREN_OPEN:
                g_queue_push_head (stack, NULL);
                expected = PAREN_OPEN | STRING;
                break;
            case PAREN_CLOSE:
                group = g_queue_pop_head (stack);
                expected = EXCLAMATION | SPACE;
                break;
            case EXCLAMATION:
                action = NEW_TASK | CONNECT;
                expected = STRING | SPACE;
                break;
            case STRING:
                {
                    gchar *s = t->str->str;

                    if (action & NEW_TASK) {
                        task = ufo_plugin_manager_get_task (pm, s, error);

                        if (task == NULL)
                            return NULL;

                        if (action & CONNECT) {
                            GList *jt;
                            guint i = 0;

                            g_list_for (group, jt)
                                ufo_task_graph_connect_nodes_full (graph, UFO_TASK_NODE (jt->data), task, i++);

                            /* create new group for this task */
                            g_queue_pop_head (stack);
                            g_queue_push_head (stack, NULL);
                        }

                        group = g_queue_pop_head (stack);
                        group = g_list_append (group, task);
                        g_queue_push_head (stack, group);

                        action = PROP_KEY;
                        expected = STRING | PAREN_CLOSE | SPACE;
                    }
                    else if (action & PROP_KEY) {
                        key = s;
                        expected = ASSIGNMENT;
                    }
                    else if (action & PROP_VALUE) {
                        set_property (task, key, s);
                        action = PROP_KEY;
                        expected = PAREN_CLOSE | COMMA | SPACE;
                    }
                }
                break;
            case COMMA:
                action = NEW_TASK;
                expected = STRING | SPACE;
                break;
            case ASSIGNMENT:
                action = PROP_VALUE;
                expected = STRING;
                break;
            case SPACE:
                expected = STRING | SPACE | PAREN_OPEN | PAREN_CLOSE | EXCLAMATION;
                break;
        }
    }

    g_queue_free (stack);
    return graph;
}

static void
progress_update (gpointer user)
{
    static int n = 0;
    g_print ("\33[2K\r%i items processed ...", ++n);
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

int
main(int argc, char* argv[])
{
    UfoPluginManager *pm;
    UfoTaskGraph *graph;
    UfoBaseScheduler *sched;
    gchar *pipeline;
    GList *tokens;
    GList *leaves;
    GOptionContext *context;
    gboolean have_tty;
    UfoResources *resources = NULL;
    GValueArray *address_list = NULL;
    GError *error = NULL;

    static gboolean quiet = FALSE;
    static gboolean trace = FALSE;
    static gchar **addresses = NULL;
    static gchar *dump = NULL;

    static GOptionEntry entries[] = {
        { "quiet", 'q', 0, G_OPTION_ARG_NONE, &quiet, "be quiet", NULL },
        { "trace", 't', 0, G_OPTION_ARG_NONE, &trace, "enable tracing", NULL },
        { "address", 'a', 0, G_OPTION_ARG_STRING_ARRAY, &addresses, "Address of remote server running `ufod'", NULL },
        { "dump", 'd', 0, G_OPTION_ARG_STRING, &dump, "Dump to JSON file", NULL },
        { NULL }
    };

#if !(GLIB_CHECK_VERSION (2, 36, 0))
    g_type_init ();
#endif

    context = g_option_context_new ("TASK [PROP=VAR [PROP=VAR ...]] ! [TASK ...]");
    g_option_context_add_main_entries (context, entries, NULL);

    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_print ("Error parsing options: %s\n", error->message);
        return 1;
    }

    if (argc == 1) {
        g_print ("%s", g_option_context_get_help (context, TRUE, NULL));
        return 0;
    }

    pm = ufo_plugin_manager_new ();
    pipeline = g_strjoinv (" ", &argv[1]);
    tokens = tokenize_args (pipeline);
    graph = parse (pipeline, tokens, pm, &error);
    g_free (pipeline);

    if (graph == NULL) {
        if (error != NULL)
            g_print ("Error parsing pipeline: %s\n", error->message);

        return 1;
    }

    leaves = ufo_graph_get_leaves (UFO_GRAPH (graph));
    have_tty = isatty (fileno (stdin));

    if (!quiet && have_tty) {
        UfoTaskNode *leaf;

        leaf = UFO_TASK_NODE (leaves->data);
        g_signal_connect (leaf, "processed", G_CALLBACK (progress_update), NULL);
    }

    sched = ufo_scheduler_new ();

    if (trace) {
        g_object_set (sched, "enable-tracing", TRUE, NULL);
    }

    address_list = string_array_to_value_array (addresses);

    if (address_list) {
        resources = UFO_RESOURCES (ufo_resources_new (NULL));
        g_object_set (G_OBJECT (resources), "remotes", address_list, NULL);
        g_value_array_free (address_list);
        ufo_base_scheduler_set_resources (sched, resources);
    }

    ufo_base_scheduler_run (sched, graph, &error);

    if (error != NULL) {
        g_print ("Error executing pipeline: %s\n", error->message);
    }

    if (!quiet) {
        gdouble run_time;

        if (have_tty)
            g_print ("\n");

        g_object_get (sched, "time", &run_time, NULL);
        g_print ("Finished in %3.5fs\n", run_time);
    }

    if (resources) {
        g_object_unref (resources);
    }

    if (dump) {
        ufo_task_graph_save_to_json (graph, dump, &error);

        if (error != NULL)
            g_print ("Error dumping task graph: %s\n", error->message);
    }

    g_list_free (leaves);

    g_object_unref (graph);
    g_object_unref (pm);
    g_strfreev (addresses);

    return 0;
}
