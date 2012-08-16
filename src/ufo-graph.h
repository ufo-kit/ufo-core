#ifndef __UFO_GRAPH_H
#define __UFO_GRAPH_H

#include <glib-object.h>
#include "ufo-plugin-manager.h"
#include "ufo-resource-manager.h"
#include "ufo-filter.h"

G_BEGIN_DECLS

#define UFO_TYPE_GRAPH             (ufo_graph_get_type())
#define UFO_GRAPH(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_GRAPH, UfoGraph))
#define UFO_IS_GRAPH(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_GRAPH))
#define UFO_GRAPH_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_GRAPH, UfoGraphClass))
#define UFO_IS_GRAPH_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_GRAPH))
#define UFO_GRAPH_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_GRAPH, UfoGraphClass))

#define UFO_GRAPH_ERROR ufo_graph_error_quark()
typedef enum {
    UFO_GRAPH_ERROR_ALREADY_LOAD,
    UFO_GRAPH_ERROR_JSON_KEY
} UfoGraphError;

typedef struct _UfoGraph           UfoGraph;
typedef struct _UfoGraphClass      UfoGraphClass;
typedef struct _UfoGraphPrivate    UfoGraphPrivate;

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

UfoGraph   *ufo_graph_new                   (const gchar        *paths);
void        ufo_graph_read_from_json        (UfoGraph           *graph,
                                             UfoPluginManager   *manager,
                                             const gchar        *filename,
                                             GError            **error);
void        ufo_graph_save_to_json          (UfoGraph           *graph,
                                             const gchar        *filename,
                                             GError            **error);
void        ufo_graph_run                   (UfoGraph           *graph,
                                             GError             **error);
void        ufo_graph_connect_filters       (UfoGraph           *graph,
                                             UfoFilter          *from,
                                             UfoFilter          *to,
                                             GError            **error);
void        ufo_graph_connect_filters_full  (UfoGraph           *graph,
                                             UfoFilter          *from,
                                             guint               from_port,
                                             UfoFilter          *to,
                                             guint               to_port,
                                             GError            **error);
UfoResourceManager *
            ufo_graph_get_resource_manager  (UfoGraph           *graph);
GType       ufo_graph_get_type              (void);
GQuark      ufo_graph_error_quark           (void);

G_END_DECLS

#endif
