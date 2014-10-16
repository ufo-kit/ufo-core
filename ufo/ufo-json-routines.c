#include <ufo/ufo-json-routines.h>

/**
 * ufo_transform_string:
 * @pattern: A pattern to place the result string in it.
 * @s: A string, which should be placed in the @pattern.
 * @separator: A string containing separator symbols in the @s that should be removed
 *
 * Returns: A string there in @pattern was placed @s.
 */
gchar *
ufo_transform_string (const gchar *pattern,
                      const gchar *s,
                      const gchar *separator)
{
    gchar **sv;
    gchar *transformed;
    gchar *result;
    sv = g_strsplit_set (s, "-_ ", -1);
    transformed = g_strjoinv (separator, sv);
    result = g_strdup_printf (pattern, transformed);
    g_strfreev (sv);
    g_free (transformed);
    return result;
}

/**
 * ufo_object_from_json:
 * @object: A #JsonObject that should be processed.
 * @manager: A #UfoPluginManager.
 *
 * Returns: (transfer full): A pointer to the object created with passed Json @object.
 */
gpointer
ufo_object_from_json (JsonObject       *object,
                      UfoPluginManager *manager)
{
    const gchar *namespace = json_object_get_string_member (object, "namespace");
    const gchar *type = json_object_get_string_member (object, "type");
    const gchar *name = json_object_get_string_member (object, "name");

    if (!namespace || !type || !name) {
        g_warning ("The object cannot be loaded because it is not identified. Please specify \"namespace\", \"type\" and \"name\"");
        return NULL;
    }

    gchar *namespace_t = ufo_transform_string ("%s", namespace, "_");
    gchar *type_t = ufo_transform_string ("%s", type, "_");
    gchar *name_t = ufo_transform_string ("%s", name, "_");

    const gchar *module_name;
    module_name = g_strdup_printf ("libufo%s_%s_%s.so", namespace_t, name_t, type_t);
    const gchar *func_name;
    func_name = g_strdup_printf ("ufo_%s_%s_%s_new", namespace_t, name_t, type_t);
    g_free (namespace_t);
    g_free (type_t);
    g_free (name_t);

    GError *error = NULL;
    gpointer plugin = ufo_plugin_manager_get_plugin (manager,
                                                     func_name,
                                                     module_name,
                                                     &error);
    if (error != NULL) {
      g_warning ("%s", error->message);
      return NULL;
    }

    if (json_object_has_member (object, "properties")) {
        JsonObject *prop_object = json_object_get_object_member (object, "properties");

        GList *members = json_object_get_members (prop_object);
        GList *member = NULL;
        gchar *member_name = NULL;

        for (member = g_list_first (members);
             member != NULL;
             member = g_list_next (member))
        {
            member_name = member->data;
            JsonNode *node = json_object_get_member(prop_object, member_name);

            if (JSON_NODE_HOLDS_VALUE (node)) {
                GValue val = {0,};
                json_node_get_value (node, &val);
                g_object_set_property (G_OBJECT (plugin), member_name, &val);
                g_value_unset (&val);
            }
            else if (JSON_NODE_HOLDS_OBJECT (node)) {
                JsonObject *inner_object = json_node_get_object (node);
                g_object_set (G_OBJECT (plugin),
                              member_name,
                              ufo_object_from_json (inner_object, manager),
                              NULL);
            }
            else {
                g_warning ("`%s' is neither a primitive value nor an object!",
                           member_name);
            }
        }
    }

    return plugin;
}
