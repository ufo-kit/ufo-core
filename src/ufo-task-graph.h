#ifndef __UFO_TASK_GRAPH_H
#define __UFO_TASK_GRAPH_H

#include <glib-object.h>
#include "ufo-graph.h"
#include "ufo-arch-graph.h"

G_BEGIN_DECLS

#define UFO_TYPE_TASK_GRAPH             (ufo_task_graph_get_type())
#define UFO_TASK_GRAPH(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_TASK_GRAPH, UfoTaskGraph))
#define UFO_IS_TASK_GRAPH(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_TASK_GRAPH))
#define UFO_TASK_GRAPH_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_TASK_GRAPH, UfoTaskGraphClass))
#define UFO_IS_TASK_GRAPH_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_TASK_GRAPH))
#define UFO_TASK_GRAPH_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_TASK_GRAPH, UfoTaskGraphClass))

typedef struct _UfoTaskGraph           UfoTaskGraph;
typedef struct _UfoTaskGraphClass      UfoTaskGraphClass;
typedef struct _UfoTaskGraphPrivate    UfoTaskGraphPrivate;
typedef struct _UfoTaskGraphConnection UfoTaskGraphConnection;

struct _UfoTaskGraphConnection {
    guint source_output;
    guint target_input;
};

/**
 * UfoTaskGraph:
 *
 * Main object for organizing filters. The contents of the #UfoTaskGraph structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoTaskGraph {
    /*< private >*/
    UfoGraph parent_instance;

    UfoTaskGraphPrivate *priv;
};

/**
 * UfoTaskGraphClass:
 *
 * #UfoTaskGraph class
 */
struct _UfoTaskGraphClass {
    /*< private >*/
    UfoGraphClass parent_class;
};

UfoGraph    *ufo_task_graph_new             (void);
void         ufo_task_graph_map             (UfoTaskGraph *task_graph,
                                             UfoArchGraph *arch_graph);
void         ufo_task_graph_split           (UfoTaskGraph *task_graph,
                                             UfoArchGraph *arch_graph);
void         ufo_task_graph_fuse            (UfoTaskGraph *task_graph);
GType        ufo_task_graph_get_type        (void);

G_END_DECLS

#endif
