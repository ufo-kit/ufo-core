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

#ifndef __UFO_GPU_NODE_H
#define __UFO_GPU_NODE_H

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <ufo/ufo-node.h>

G_BEGIN_DECLS

#define UFO_TYPE_GPU_NODE             (ufo_gpu_node_get_type())
#define UFO_GPU_NODE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_GPU_NODE, UfoGpuNode))
#define UFO_IS_GPU_NODE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_GPU_NODE))
#define UFO_GPU_NODE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_GPU_NODE, UfoGpuNodeClass))
#define UFO_IS_GPU_NODE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_GPU_NODE))
#define UFO_GPU_NODE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_GPU_NODE, UfoGpuNodeClass))

typedef struct _UfoGpuNode           UfoGpuNode;
typedef struct _UfoGpuNodeClass      UfoGpuNodeClass;
typedef struct _UfoGpuNodePrivate    UfoGpuNodePrivate;

/**
 * UfoGpuNode:
 *
 * Main object for organizing filters. The contents of the #UfoGpuNode structure
 * are private and should only be accessed via the provided API.
 */
struct _UfoGpuNode {
    /*< private >*/
    UfoNode parent_instance;

    UfoGpuNodePrivate *priv;
};

/**
 * UfoGpuNodeClass:
 *
 * #UfoGpuNode class
 */
struct _UfoGpuNodeClass {
    /*< private >*/
    UfoNodeClass parent_class;
};

/**
 * UfoGpuNodeInfo:
 * @UFO_GPU_NODE_INFO_GLOBAL_MEM_SIZE: Global memory size
 * @UFO_GPU_NODE_INFO_LOCAL_MEM_SIZE: Local memory size
 *
 * OpenCL device info types. Refer to the OpenCL standard for complete details
 * about each information.
 */
typedef enum {
    UFO_GPU_NODE_INFO_GLOBAL_MEM_SIZE = 0,
    UFO_GPU_NODE_INFO_LOCAL_MEM_SIZE
} UfoGpuNodeInfo;

UfoNode  *ufo_gpu_node_new              (gpointer        context,
                                         gpointer        device);
gpointer  ufo_gpu_node_get_cmd_queue    (UfoGpuNode     *node);
GValue   *ufo_gpu_node_get_info         (UfoGpuNode     *node,
                                         UfoGpuNodeInfo  info);
GType     ufo_gpu_node_get_type         (void);

G_END_DECLS

#endif
