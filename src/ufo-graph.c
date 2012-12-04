#include <stdio.h>
#include <ufo-node.h>
#include <ufo-graph.h>

G_DEFINE_TYPE (UfoGraph, ufo_graph, G_TYPE_OBJECT)

#define UFO_GRAPH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_GRAPH, UfoGraphPrivate))

typedef struct {
    UfoNode     *target;
    gpointer     label;
} UfoEdge;

struct _UfoGraphPrivate {
    GHashTable *adjacency;
    GList *node_types;
    GList *nodes;
    guint n_edges;
};

enum {
    PROP_0,
    N_PROPERTIES
};

UfoGraph *
ufo_graph_new (void)
{
    return UFO_GRAPH (g_object_new (UFO_TYPE_GRAPH, NULL));
}

void
ufo_graph_register_node_type (UfoGraph *graph,
                              GType type)
{
    UfoGraphPrivate *priv;

    g_return_if_fail (UFO_IS_GRAPH (graph));

    /* Make sure type is at least a node type */
    g_return_if_fail (g_type_is_a (type, UFO_TYPE_NODE));

    priv = graph->priv;
    priv->node_types = g_list_append (priv->node_types, GINT_TO_POINTER (type));
}

/**
 * ufo_graph_get_registered_node_types:
 * @graph: A #UfoGraph
 *
 * Returns: (element-type GType) (transfer container): A list of #GType
 * identifiers that can be added to @graph.
 */
GList *
ufo_graph_get_registered_node_types (UfoGraph *graph)
{
    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    return g_list_copy (graph->priv->node_types);
}

gboolean
ufo_graph_is_connected (UfoGraph *graph,
                        UfoNode *from,
                        UfoNode *to)
{
    UfoGraphPrivate *priv;
    GList *successors;
    gboolean found = FALSE;

    g_return_val_if_fail (UFO_IS_GRAPH (graph), FALSE);

    priv = graph->priv;
    successors = g_hash_table_lookup (priv->adjacency, from);

    for (GList *it = g_list_first (successors); it != NULL; it = g_list_next (it)) {
        UfoEdge *edge = (UfoEdge *) it->data;

        if (edge->target == to) {
            found = TRUE;
            break;
        }
    }

    return found;
}

static gboolean
is_valid_node_type (UfoGraphPrivate *priv,
                    UfoNode *node)
{
    for (GList *it = g_list_first (priv->node_types); it != NULL; it = g_list_next (it)) {
        GType type = (GType) GPOINTER_TO_INT (it->data);

        if (G_TYPE_CHECK_INSTANCE_TYPE (node, type))
            return TRUE;
    }

    return FALSE;
}

static void
add_node_if_not_found (UfoGraphPrivate *priv,
                       UfoNode *node)
{
    if (!g_list_find (priv->nodes, node))
        priv->nodes = g_list_append (priv->nodes, node);
}

void
ufo_graph_connect_nodes (UfoGraph *graph,
                         UfoNode *source,
                         UfoNode *target,
                         gpointer edge_label)
{
    UfoGraphPrivate *priv;
    UfoEdge *edge;
    GList *successors;

    g_return_if_fail (UFO_IS_GRAPH (graph));
    priv = graph->priv;

    g_return_if_fail (is_valid_node_type (priv, source) && is_valid_node_type (priv, target));

    edge = g_new0 (UfoEdge, 1);
    edge->target = target;
    edge->label = edge_label;

    successors = g_hash_table_lookup (priv->adjacency, source);
    successors = g_list_append (successors, edge);
    g_hash_table_insert (priv->adjacency, source, successors);

    add_node_if_not_found (priv, source);
    add_node_if_not_found (priv, target);

    priv->n_edges++;
}

guint
ufo_graph_get_num_nodes (UfoGraph *graph)
{
    g_return_val_if_fail (UFO_IS_GRAPH (graph), 0);
    return g_list_length (graph->priv->nodes);
}

guint
ufo_graph_get_num_edges (UfoGraph *graph)
{
    g_return_val_if_fail (UFO_IS_GRAPH (graph), 0);
    return graph->priv->n_edges;
}

/**
 * ufo_graph_get_nodes:
 * @graph: A #UfoGraph
 *
 * Returns: (element-type UfoNode) (transfer container): A list of all nodes
 * added to @graph.
 */
GList *
ufo_graph_get_nodes (UfoGraph *graph)
{
    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    return graph->priv->nodes;
}

/**
 * ufo_graph_get_nodes_filtered:
 * @graph: A #UfoGraph
 * @func: Predicate function to filter out nodes
 *
 * Returns: (element-type UfoNode) (transfer container): A list of all nodes
 * that are marked as true by the predicate function @func.
 */
GList *
ufo_graph_get_nodes_filtered (UfoGraph *graph,
                              UfoFilterPredicate func)
{
    UfoGraphPrivate *priv;
    GList *result = NULL;

    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    priv = graph->priv;

    for (GList *it = g_list_first (priv->nodes); it != NULL; it = g_list_next (it)) {
        UfoNode *node = UFO_NODE (it->data);

        if (func (node))
            result = g_list_append (result, node);
    }

    return result;
}

void
ufo_graph_remove_edge (UfoGraph *graph,
                       UfoNode *source,
                       UfoNode *target)
{
    UfoGraphPrivate *priv;
    GList *edges;

    g_return_if_fail (UFO_IS_GRAPH (graph));
    priv = graph->priv;

    edges = g_hash_table_lookup (priv->adjacency, source);

    if (edges != NULL) {
        UfoEdge *edge = NULL;

        for (GList *it = g_list_first (edges); it != NULL; it = g_list_next (it)) {
            UfoEdge *candidate = (UfoEdge *) it->data;

            if (candidate->target == target) {
                edge = candidate;
                break;
            }
        }

        if (edge != NULL) {
            priv->nodes = g_list_remove (priv->nodes, source);
            g_object_unref (source);

            priv->nodes = g_list_remove (priv->nodes, edge->target);
            g_object_unref (edge->target);

            edges = g_list_remove (edges, edge);
            g_hash_table_insert (priv->adjacency, source, edges);
        }
    }

    priv->n_edges--;
}

gpointer
ufo_graph_get_edge_label (UfoGraph *graph,
                          UfoNode *source,
                          UfoNode *target)
{
    UfoGraphPrivate *priv;
    GList *edges;

    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    priv = graph->priv;
    edges = g_hash_table_lookup (priv->adjacency, source);

    for (GList *it = g_list_first (edges); it != NULL; it = g_list_next (it)) {
        UfoEdge *edge = (UfoEdge *) it->data;

        if (edge->target == target) {
            gpointer label = edge->label;
            return label;
        }
    }

    g_warning ("target not found");
    return NULL;
}

static gboolean
has_predecessor (UfoGraph *graph,
                 UfoNode *node)
{
    for (GList *it = g_list_first (graph->priv->nodes); it != NULL; it = g_list_next (it)) {
        UfoNode *source = (UfoNode *) it->data;

        if (ufo_graph_is_connected (graph, source, node))
            return TRUE;
    }

    return FALSE;
}

/**
 * ufo_graph_get_roots:
 * @graph: A #UfoGraph
 *
 * Returns: (element-type UfoNode) (transfer container): A list of all nodes
 * that do not have a predessor node.
 */
GList *
ufo_graph_get_roots (UfoGraph *graph)
{
    UfoGraphPrivate *priv;
    GList *result;

    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    priv = graph->priv;
    result = NULL;

    for (GList *it = g_list_first (priv->nodes); it != NULL; it = g_list_next (it)) {
        UfoNode *node = (UfoNode *) it->data;

        if (!has_predecessor (graph, node))
            result = g_list_append (result, node);
    }

    return result;
}

/**
 * ufo_graph_get_successors:
 * @graph: A #UfoGraph
 *
 * Returns: (element-type UfoNode) (transfer container): All succeeding nodes of
 * @node.
 */
GList *
ufo_graph_get_successors (UfoGraph *graph,
                          UfoNode *node)
{
    UfoGraphPrivate *priv;
    GList *edges;
    GList *result;

    g_return_val_if_fail (UFO_IS_GRAPH (graph), NULL);
    priv = graph->priv;
    edges = g_hash_table_lookup (priv->adjacency, node);
    result = NULL;

    for (GList *it = g_list_first (edges); it != NULL; it = g_list_next (it)) {
        UfoEdge *edge = (UfoEdge *) it->data;
        result = g_list_prepend (result, edge->target);
    }

    return result;
}

/**
 * ufo_graph_split:
 * @graph: A #UfoGraph
 * @path: (element-type UfoNode): A path of nodes, preferably created with
 * ufo_graph_get_paths().
 *
 * Duplicate nodes between head and tail of path and insert at the exact the
 * position of where path started and ended.
 */
void
ufo_graph_split (UfoGraph *graph,
                 GList *path)
{
    GList *head;
    GList *tail;
    UfoNode *orig;
    UfoNode *current;
    GError *error = NULL;

    g_return_if_fail (UFO_IS_GRAPH (graph));

    head = g_list_first (path);
    tail = g_list_last (path);
    g_assert (head != tail);

    orig = UFO_NODE (head->data);

    /* The first link goes from the original head */
    current = orig;

    for (GList *it = g_list_next (head); it != tail; it = g_list_next (it)) {
        UfoNode *next;
        UfoNode *copy;
        gpointer label;

        next = UFO_NODE (it->data);
        copy = ufo_node_copy (next, &error);
        label = ufo_graph_get_edge_label (graph, orig, next);
        ufo_graph_connect_nodes (graph, current, copy, label);
        current = copy;
        orig = next;
    }

    if (tail->data != NULL) {
        ufo_graph_connect_nodes (graph, current, UFO_NODE (tail->data),
                                 ufo_graph_get_edge_label (graph, orig, UFO_NODE (tail->data)));
    }
}

static void
pickup_paths (UfoGraph *graph,
              UfoFilterPredicate pred,
              UfoNode *current,
              UfoNode *last,
              GList  *current_path,
              GList **paths)
{
    GList *successors;

    if (pred (current)) {
        if (!pred (last))
            current_path = g_list_append (current_path, last);

        current_path = g_list_append (current_path, current);
    }
    else {
        if (current_path != NULL) {
            current_path = g_list_append (current_path, current);
            *paths = g_list_append (*paths, current_path);
        }

        current_path = NULL;
    }

    successors = ufo_graph_get_successors (graph, current);

    for (GList *it = g_list_first (successors); it != NULL; it = g_list_next (it))
        pickup_paths (graph, pred, it->data, current, g_list_copy (current_path), paths);

    g_list_free (successors);
}

/**
 * ufo_graph_get_paths:
 * @graph: A #UfoGraph
 * @pred: A predicate function
 *
 * Compute a list of lists that contain complete paths with nodes that match a
 * predicate function.
 *
 * Returns: (element-type GLib.GList) (transfer full): A list of lists with paths
 * that match @pred.
 */
GList *
ufo_graph_get_paths (UfoGraph *graph,
                     UfoFilterPredicate pred)
{
    GList *roots;
    GList *paths = NULL;

    roots = ufo_graph_get_roots (graph);

    for (GList *it = g_list_first (roots); it != NULL; it = g_list_next (it)) {
        UfoNode *node = UFO_NODE (it->data);
        pickup_paths (graph, pred, node, node, NULL, &paths);
    }

    g_list_free (roots);
    return paths;
}

void
ufo_graph_dump_dot (UfoGraph *graph,
                    const gchar *filename)
{
    FILE *fp;
    GList *nodes;

    fp = fopen (filename, "w");
    fprintf (fp, "digraph foo {\n");

    nodes = ufo_graph_get_nodes (graph);

    for (GList *it = g_list_first (nodes); it != NULL; it = g_list_next (it)) {
        UfoNode *source;
        GList *successors;

        source = UFO_NODE (it->data);
        successors = ufo_graph_get_successors (graph, source);

        for (GList *jt = g_list_first (successors); jt != NULL; jt = g_list_next (jt)) {
            UfoNode *target;

            target = UFO_NODE (jt->data);

            fprintf (fp, "  %s_%p -> %s_%p;\n",
                     g_type_name (G_TYPE_FROM_INSTANCE (source)), (gpointer) source,
                     g_type_name (G_TYPE_FROM_INSTANCE (target)), (gpointer) target);
        }

        g_list_free (successors);
    }

    fprintf (fp, "}\n");
    fclose (fp);
}

static void
ufo_graph_dispose (GObject *object)
{
    UfoGraphPrivate *priv;

    priv = UFO_GRAPH_GET_PRIVATE (object);

    if (priv->adjacency != NULL) {
        g_hash_table_destroy (priv->adjacency);
        priv->adjacency = NULL;
    }

    g_list_foreach (priv->nodes, (GFunc) g_object_unref, NULL);

    G_OBJECT_CLASS (ufo_graph_parent_class)->dispose (object);
}

static void
ufo_graph_finalize (GObject *object)
{
    UfoGraphPrivate *priv;

    priv = UFO_GRAPH_GET_PRIVATE (object);

    if (priv->nodes) {
        g_list_free (priv->nodes);
        priv->nodes = NULL;
    }

    G_OBJECT_CLASS (ufo_graph_parent_class)->finalize (object);
}

static void
ufo_graph_class_init (UfoGraphClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->dispose  = ufo_graph_dispose;
    oclass->finalize = ufo_graph_finalize;

    g_type_class_add_private(klass, sizeof(UfoGraphPrivate));
}

static void
ufo_graph_init (UfoGraph *self)
{
    UfoGraphPrivate *priv;
    self->priv = priv = UFO_GRAPH_GET_PRIVATE (self);
    priv->adjacency = g_hash_table_new (g_direct_hash, g_direct_equal);
    priv->node_types = NULL;
    priv->nodes = NULL;
    priv->n_edges = 0;
}
