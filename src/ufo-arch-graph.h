#ifndef __UFO_ARCH_GRAPH_H
#define __UFO_ARCH_GRAPH_H

#include <glib-object.h>
#include <ufo-graph.h>
#include <ufo-resources.h>

G_BEGIN_DECLS

#define UFO_TYPE_ARCH_GRAPH             (ufo_arch_graph_get_type())
#define UFO_ARCH_GRAPH(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_ARCH_GRAPH, UfoArchGraph))
#define UFO_IS_ARCH_GRAPH(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_ARCH_GRAPH))
#define UFO_ARCH_GRAPH_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_ARCH_GRAPH, UfoArchGraphClass))
#define UFO_IS_ARCH_GRAPH_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_ARCH_GRAPH))
#define UFO_ARCH_GRAPH_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_ARCH_GRAPH, UfoArchGraphClass))

typedef struct _UfoArchGraph           UfoArchGraph;
typedef struct _UfoArchGraphClass      UfoArchGraphClass;
typedef struct _UfoArchGraphPrivate    UfoArchGraphPrivate;

/**
 * UfoArchGraph:
 *
 * Graph structure that describes the relation between hardware nodes. The
 * contents of the #UfoArchGraph structure are private and should only be
 * accessed via the provided API.
 */
struct _UfoArchGraph {
    /*< private >*/
    UfoGraph parent_instance;

    UfoArchGraphPrivate *priv;
};

/**
 * UfoArchGraphClass:
 *
 * #UfoArchGraph class
 */
struct _UfoArchGraphClass {
    /*< private >*/
    UfoGraphClass parent_class;
};

UfoGraph    *ufo_arch_graph_new                 (gpointer        zmq_context,
                                                 GList          *remote_addresses,
                                                 UfoResources   *resources);
gpointer     ufo_arch_graph_get_context         (UfoArchGraph   *graph);
guint        ufo_arch_graph_get_num_cpus        (UfoArchGraph   *graph);
guint        ufo_arch_graph_get_num_gpus        (UfoArchGraph   *graph);
GList       *ufo_arch_graph_get_gpu_nodes       (UfoArchGraph   *graph);
GList       *ufo_arch_graph_get_remote_nodes    (UfoArchGraph   *graph);
GType        ufo_arch_graph_get_type            (void);

G_END_DECLS

#endif
