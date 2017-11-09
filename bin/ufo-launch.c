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

#include "config.h"



static gboolean
str_to_boolean (const gchar *s)
{
    return g_ascii_strncasecmp (s, "true", 4) == 0;
}

static void
value_transform_enum (const GValue *src_value, GValue *dest_value)
{
    GEnumClass *enum_class;
    GEnumValue *enum_value;
    const gchar *str_value;

    str_value = g_value_get_string (src_value);
    enum_class = g_type_class_ref (G_VALUE_TYPE (dest_value));
    enum_value = g_enum_get_value_by_name (enum_class, str_value);
    enum_value = enum_value == NULL ? g_enum_get_value_by_nick (enum_class, str_value) : enum_value;

    if (enum_value != NULL)
        g_value_set_enum (dest_value, enum_value->value);
    else
        g_warning ("%s does not have an enum value %s",
                   G_VALUE_TYPE_NAME (dest_value), str_value);

    g_type_class_unref (enum_class);
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
    COMMA       = 1 << 6,
    STOP        = 1 << 7
} TokenType;

typedef struct {
    TokenType type;
    GString *str;
    guint pos;
} Token;

typedef struct {
    UfoPluginManager *pm;
    UfoTaskGraph *graph;
    GList *current;
    GError **error;
} Environment;


static GList *
tokenize_args (const gchar *pipeline)
{
    GList *tokens = NULL;
    Token *current = NULL;
    guint pos = 0;
    gboolean inside_quote = FALSE;

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

        if (*s == '\'') {
            inside_quote = !inside_quote;
            continue;
        }

        for (; map[i].c != 0 && !inside_quote; i++) {
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
        if (map[i].c == 0 || inside_quote) {
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

    current = g_new0 (Token, 1);
    current->type = STOP;

    tokens = g_list_append (tokens, current);

    return tokens;
}

static void
free_tokens (Token *token)
{
    if (token->str != NULL)
        g_string_free (token->str, TRUE);

    g_free (token);
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

    g_value_unset (&value);
}

static Token *
token (GList *it)
{
    return (it == NULL) ? NULL : (Token *) it->data;
}

static Token *
consume (Environment *env)
{
    Token *t = token (env->current);
    env->current = env->current->next;
    return t;
}

static Token *
peek (Environment *env)
{
    return token (env->current);
}

static void
skip (Environment *env)
{
    env->current = env->current->next;
}

static void
consume_spaces (Environment *env)
{
    while (env->current != NULL && (token (env->current))->type == SPACE)
        env->current = env->current->next;
}

static gboolean
consume_maybe (Environment *env, TokenType type)
{
    gboolean result;
    GList *tmp;

    consume_spaces (env);
    tmp = env->current;
    result = env->current == NULL ? FALSE : (consume (env))->type == type;

    if (!result)
        env->current = tmp;

    return result;
}

static UfoTaskNode * read_connection (Environment *env, UfoTaskNode *previous);

static gboolean
try_consume_assignment (Environment *env, UfoTaskNode *task)
{
    GParamSpec *pspec;
    Token *key;
    Token *equal;
    Token *value;

    if (env->current == NULL)
        return FALSE;

    key = consume (env);

    if (key->type != STRING)
        return FALSE;

    equal = consume (env);

    if (equal == NULL || equal->type != ASSIGNMENT)
        return FALSE;

    pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (task), key->str->str);

    if (pspec != NULL && pspec->value_type == G_TYPE_VALUE_ARRAY) {
        GParamSpecValueArray *vapspec;
        GValueArray *array;
        GValue array_value = { 0, };
        Token *next;

        vapspec = (GParamSpecValueArray *) pspec;
        array = g_value_array_new (8);

        do {
            GValue from_value = { 0, };
            GValue to_value = { 0, };

            value = consume (env);

            if (value == NULL || value->type != STRING)
                return FALSE;

            next = consume (env);

            g_value_init (&from_value, G_TYPE_STRING);
            g_value_init (&to_value, vapspec->element_spec->value_type);
            g_value_set_string (&from_value, value->str->str);
            g_value_transform (&from_value, &to_value);
            g_value_array_append (array, &to_value);
        } while (next->type == COMMA);

        g_value_init (&array_value, G_TYPE_VALUE_ARRAY);
        g_value_set_boxed (&array_value, array);
        g_object_set_property (G_OBJECT (task), key->str->str, &array_value);
    }
    else {
        value = consume (env);

        if (value == NULL || key->type != STRING)
            return FALSE;

        set_property (task, key->str->str, value->str->str);
    }

    return TRUE;
}

static UfoTaskNode *
try_consume_task (Environment *env)
{
    Token *t;
    UfoTaskNode *node = NULL;

    t = consume (env);

    if (t->type != STRING)
        return NULL;

    node = ufo_plugin_manager_get_task (env->pm, t->str->str, env->error);

    if (node == NULL)
        return NULL;

    consume_spaces (env);

    while (1) {
        GList *tmp;

        tmp = env->current;

        if (!try_consume_assignment (env, node)) {
            env->current = tmp;
            break;
        }

        consume_spaces (env);
    }

    return node;
}

static GList *
read_params (Environment *env)
{
    /*
     * params is something like "[A, B ! C, D]". The returned list should
     * contain [A, C, D] which are the actual output streams for the subsequent
     * task.
     */
    GList *result = NULL;

    if (peek (env)->type != PAREN_OPEN)
        return NULL;

    skip (env);

    while ((*env->error) == NULL) {
        UfoTaskNode *previous = NULL;
        UfoTaskNode *last = NULL;

        if (peek (env)->type == STOP) {
            g_set_error_literal (env->error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                                 "Expected ',', ']', new task or task property.");
            break;
        }

        if (peek (env)->type == PAREN_CLOSE) {
            consume (env);
            break;
        }

        if (peek (env)->type == COMMA) {
            consume (env);
            continue;
        }

        do {
            last = previous;
            previous = read_connection (env, previous);
        } while (previous != NULL && *(env->error) == NULL);

        result = g_list_append (result, last);
    }

    return result;
}

static UfoTaskNode *
read_connection (Environment *env, UfoTaskNode *previous)
{
    UfoTaskNode *next;
    GList *params = NULL;

    if (env->current == NULL)
        return NULL;

    consume_spaces (env);

    if (peek (env)->type != PAREN_OPEN && peek (env)->type != STRING)
        return NULL;

    params = read_params (env);
    previous = try_consume_task (env);

    if (*(env->error) != NULL)
        return NULL;

    do {
        if (consume_maybe (env, EXCLAMATION)) {
            consume_spaces (env);
            next = try_consume_task (env);

            if (next == NULL)
                return previous;

            if (params == NULL) {
                ufo_task_graph_connect_nodes (env->graph, previous, next);
            }
            else {
                GList *it;
                guint i = 0;
                g_list_for (params, it) {
                    UfoTaskNode *from;

                    from = UFO_TASK_NODE (it->data);
                    ufo_task_graph_connect_nodes_full (env->graph, from, next, i++);
                }

                g_list_free (params);
                params = NULL;
            }

            previous = next;
        }
        else {
            next = NULL;
        }
    }
    while (next != NULL);

    return previous;
}

static UfoTaskGraph *
parse (const gchar *pipeline, GList *tokens, UfoPluginManager *pm, GError **error)
{
    UfoTaskNode *previous = NULL;
    Environment env;

    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_UCHAR,   value_transform_uchar);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_INT,     value_transform_int);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_UINT,    value_transform_uint);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_UINT64,  value_transform_uint64);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_LONG,    value_transform_long);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_ULONG,   value_transform_ulong);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_FLOAT,   value_transform_float);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_DOUBLE,  value_transform_double);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_BOOLEAN, value_transform_boolean);
    g_value_register_transform_func (G_TYPE_STRING, G_TYPE_ENUM,    value_transform_enum);

    env.current = tokens;
    env.pm = pm;
    env.graph = UFO_TASK_GRAPH (ufo_task_graph_new ());
    env.error = error;

    do {
        previous = read_connection (&env, previous);
    } while (previous != NULL && *(env.error) == NULL);

    return (*env.error != NULL) ? NULL : env.graph;
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
    GList *nodes;
    GList *it;
    GOptionContext *context;
    gboolean have_tty;
    UfoResources *resources = NULL;
    GValueArray *address_list = NULL;
    GError *error = NULL;

    static gboolean quiet = FALSE;
    static gboolean quieter = FALSE;
    static gboolean trace = FALSE;
    static gboolean version = FALSE;
    static gboolean timestamps = FALSE;
    static gchar **addresses = NULL;
    static gchar *dump = NULL;

    static GOptionEntry entries[] = {
        { "trace",   't', 0, G_OPTION_ARG_NONE, &trace, "enable tracing", NULL },
        { "address", 'a', 0, G_OPTION_ARG_STRING_ARRAY, &addresses, "Address of remote server running `ufod'", NULL },
        { "dump",    'd', 0, G_OPTION_ARG_STRING, &dump, "Dump to JSON file", NULL },
        { "timestamps",0, 0, G_OPTION_ARG_NONE, &timestamps, "generate timestamps", NULL },
        { "quiet",   'q', 0, G_OPTION_ARG_NONE, &quiet, "be quiet", NULL },
        { "quieter",   0, 0, G_OPTION_ARG_NONE, &quieter, "be quieter", NULL },
        { "version",   0, 0, G_OPTION_ARG_NONE, &version, "Show version information", NULL },
        { NULL }
    };

#if !(GLIB_CHECK_VERSION (2, 36, 0))
    g_type_init ();
#endif

    context = g_option_context_new ("TASK [PROP=VAR [PROP=VAR ...]] ! [TASK ...]");
    g_option_context_add_main_entries (context, entries, NULL);

    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_printerr ("Error parsing options: %s\n", error->message);
        return 1;
    }

    if (version) {
        g_print ("%s version " UFO_VERSION "\n", argv[0]);
        return 0;
    }

    quiet = quiet || quieter;

    pm = ufo_plugin_manager_new ();
    pipeline = g_strjoinv (" ", &argv[1]);
    tokens = tokenize_args (pipeline);
    graph = parse (pipeline, tokens, pm, &error);
    g_free (pipeline);
    g_list_free_full (tokens, (GDestroyNotify) free_tokens);

    if (graph == NULL) {
        if (error != NULL)
            g_printerr ("Error parsing pipeline: %s\n", error->message);

        return 1;
    }

    leaves = ufo_graph_get_leaves (UFO_GRAPH (graph));

    if (leaves == NULL) {
        g_print ("%s", g_option_context_get_help (context, TRUE, NULL));
        return 0;
    }

    nodes = ufo_graph_get_nodes (UFO_GRAPH (graph));
    have_tty = isatty (fileno (stdin));

    if (!quiet && have_tty) {
        UfoTaskNode *leaf;

        leaf = UFO_TASK_NODE (leaves->data);
        g_signal_connect (leaf, "processed", G_CALLBACK (progress_update), NULL);
    }

    sched = ufo_scheduler_new ();

    g_object_set (sched,
                  "enable-tracing", trace,
                  "timestamps", timestamps,
                  NULL);

    address_list = string_array_to_value_array (addresses);

    if (address_list) {
        resources = UFO_RESOURCES (ufo_resources_new (NULL));
        g_object_set (G_OBJECT (resources), "remotes", address_list, NULL);
        g_value_array_free (address_list);
        ufo_base_scheduler_set_resources (sched, resources);
    }

    if (!dump)
        ufo_base_scheduler_run (sched, graph, &error);

    if (error != NULL) {
        g_printerr ("Error executing pipeline: %s\n", error->message);
        return 1;
    }

    if (!quieter) {
        gdouble run_time;

        if (!quiet && have_tty)
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
            g_printerr ("Error dumping task graph: %s\n", error->message);
    }

    g_list_for (nodes, it)
        g_object_unref (G_OBJECT (it->data));

    g_list_free (leaves);
    g_list_free (nodes);

    g_object_unref (graph);
    g_object_unref (sched);
    g_object_unref (pm);
    g_strfreev (addresses);
    g_option_context_free (context);

    return 0;
}
