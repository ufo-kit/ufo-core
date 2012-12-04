#ifndef __UFO_GRAPH_H
#define __UFO_GRAPH_H

#include <glib-object.h>
#include "ufo-node.h"

G_BEGIN_DECLS

#define UFO_TYPE_GRAPH             (ufo_graph_get_type())
#define UFO_GRAPH(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_GRAPH, UfoGraph))
#define UFO_IS_GRAPH(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_GRAPH))
#define UFO_GRAPH_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_GRAPH, UfoGraphClass))
#define UFO_IS_GRAPH_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_GRAPH))
#define UFO_GRAPH_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_GRAPH, UfoGraphClass))

typedef struct _UfoGraph           UfoGraph;
typedef struct _UfoGraphClass      UfoGraphClass;
typedef struct _UfoGraphPrivate    UfoGraphPrivate;

typedef gboolean (*UfoFilterPredicate) (UfoNode *node);

/**
 * UfoGraph:
 *
 * Main object for organizing filters. The contents of the #UfoGraph structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoGraph {
    /*< private >*/
    GObject parent_instance;

    UfoGraphPrivate *priv;
};

/**
 * UfoGraphClass:
 *
 * #UfoGraph class
 */
struct _UfoGraphClass {
    /*< private >*/
    GObjectClass parent_class;
};

UfoGraph   *ufo_graph_new                   (void);
void        ufo_graph_register_node_type    (UfoGraph       *graph,
                                             GType           type);
GList      *ufo_graph_get_registered_node_types
                                            (UfoGraph       *graph);
void        ufo_graph_connect_nodes         (UfoGraph       *graph,
                                             UfoNode        *source,
                                             UfoNode        *target,
                                             gpointer        edge_label);
gboolean    ufo_graph_is_connected          (UfoGraph       *graph,
                                             UfoNode        *from,
                                             UfoNode        *to);
void        ufo_graph_remove_edge           (UfoGraph       *graph,
                                             UfoNode        *source,
                                             UfoNode        *target);
gpointer    ufo_graph_get_edge_label        (UfoGraph       *graph,
                                             UfoNode        *source,
                                             UfoNode        *target);
guint       ufo_graph_get_num_nodes         (UfoGraph       *graph);
guint       ufo_graph_get_num_edges         (UfoGraph       *graph);
GList      *ufo_graph_get_nodes             (UfoGraph       *graph);
GList      *ufo_graph_get_nodes_filtered    (UfoGraph       *graph,
                                             UfoFilterPredicate func);
GList      *ufo_graph_get_roots             (UfoGraph       *graph);
GList      *ufo_graph_get_successors        (UfoGraph       *graph,
                                             UfoNode        *node);
GList      *ufo_graph_get_paths             (UfoGraph       *graph,
                                             UfoFilterPredicate pred);
void        ufo_graph_split                 (UfoGraph       *graph,
                                             GList          *path);
void        ufo_graph_dump_dot              (UfoGraph       *graph,
                                             const gchar    *filename);
GType       ufo_graph_get_type              (void);

G_END_DECLS

#endif
