/*
 * Copyright (C) 2011-2013 Karlsruhe Institute of Technology
 *
 * This file is part of Ufo.
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __UFO_ARCH_GRAPH_H
#define __UFO_ARCH_GRAPH_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <ufo/ufo-graph.h>
#include <ufo/ufo-resources.h>

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

UfoGraph     *ufo_arch_graph_new              (UfoResources   *resources,
                                               GValueArray    *remotes);
UfoResources *ufo_arch_graph_get_resources    (UfoArchGraph   *graph);
guint         ufo_arch_graph_get_num_cpus     (UfoArchGraph   *graph);
guint         ufo_arch_graph_get_num_remotes  (UfoArchGraph   *graph);
GList        *ufo_arch_graph_get_remote_nodes (UfoArchGraph   *graph);
GType         ufo_arch_graph_get_type         (void);

G_END_DECLS

#endif
