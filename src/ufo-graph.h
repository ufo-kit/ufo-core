#ifndef __UFO_GRAPH_H
#define __UFO_GRAPH_H

#include <glib-object.h>
#include "ufo-container.h"

#define UFO_TYPE_GRAPH             (ufo_graph_get_type())
#define UFO_GRAPH(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_GRAPH, UfoGraph))
#define UFO_IS_GRAPH(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_GRAPH))
#define UFO_GRAPH_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_GRAPH, UfoGraphClass))
#define UFO_IS_GRAPH_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_GRAPH))
#define UFO_GRAPH_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_GRAPH, UfoGraphClass))

typedef struct _UfoGraph           UfoGraph;
typedef struct _UfoGraphClass      UfoGraphClass;
typedef struct _UfoGraphPrivate    UfoGraphPrivate;

struct _UfoGraph {
    GObject parent_instance;

    UfoGraphPrivate *priv;
};

struct _UfoGraphClass {
    GObjectClass parent_class;
};

UfoGraph *ufo_graph_new();
void ufo_graph_read_from_json(UfoGraph *graph, const gchar *filename, GError **error);
void ufo_graph_run(UfoGraph *self);
guint ufo_graph_get_number_of_devices(UfoGraph *graph);
GList *ufo_graph_get_filter_names(UfoGraph *graph);

UfoFilter *ufo_graph_get_filter(UfoGraph *self, const gchar *plugin_name, GError **error);
void ufo_graph_add_filter(UfoGraph *graph, UfoFilter *filter, const char *name);

GType ufo_graph_get_type(void);

#endif
