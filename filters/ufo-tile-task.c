/*
 * Copyright (C) 2011-2015 Karlsruhe Institute of Technology
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
#include "config.h"

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "ufo-tile-task.h"


struct _UfoTileTaskPrivate {
    guint       width;
    guint       height;
    guint       in_width;
    guint       in_height;
    guint       x;
    guint       y;
    guint       num_horizontal;
    guint       num_vertical;
    cl_context  context;
    cl_mem      temp;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoTileTask, ufo_tile_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_TILE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_TILE_TASK, UfoTileTaskPrivate))

enum {
    PROP_0,
    PROP_WIDTH,
    PROP_HEIGHT,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_tile_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_TILE_TASK, NULL));
}

static void
ufo_tile_task_setup (UfoTask *task,
                     UfoResources *resources,
                     GError **error)
{
    UfoTileTaskPrivate *priv;
    
    priv = UFO_TILE_TASK_GET_PRIVATE (task);
    priv->context = ufo_resources_get_context (resources);

    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);
}

static void
ufo_tile_task_get_requisition (UfoTask *task,
                               UfoBuffer **inputs,
                               UfoRequisition *requisition,
                               GError **error)
{
    UfoTileTaskPrivate *priv;
    gsize in_width;
    gsize in_height;
    
    priv = UFO_TILE_TASK_GET_PRIVATE (task);
    ufo_buffer_get_requisition (inputs[0], requisition);
    in_width = requisition->dims[0];
    in_height = requisition->dims[1];

    if (priv->width > 0) {
        if (priv->width > in_width)
            g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_GET_REQUISITION,
                         "tile: width %u cannot be larger than %zu", priv->width, in_width);
        else if (in_width % priv->width)
            g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_GET_REQUISITION,
                         "tile: input width %zu must be a multiple of width %u", in_width, priv->width);
        else
            requisition->dims[0] = priv->width;
    }

    if (priv->height > 0) {
        if (priv->height > in_height)
            g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_GET_REQUISITION,
                         "tile: height %u cannot be larger than %zu", priv->height, in_height);
        else if (requisition->dims[1] % priv->height)
            g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_GET_REQUISITION,
                         "tile: input height %zu must be a multiple of height %u", in_height, priv->height);
        else
            requisition->dims[1] = priv->height;
    }

    if (priv->temp == NULL) {
        cl_int err_code;

        priv->temp = clCreateBuffer (priv->context, CL_MEM_READ_WRITE, ufo_buffer_get_size (inputs[0]), NULL, &err_code);
        UFO_RESOURCES_CHECK_SET_AND_RETURN (err_code, error);
    }

    priv->num_horizontal = in_width / requisition->dims[0];
    priv->num_vertical = in_height / requisition->dims[1];
    priv->in_width = (guint) in_width;
    priv->in_height = (guint) in_height;
}

static guint
ufo_tile_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_tile_task_get_num_dimensions (UfoTask *task,
                                  guint input)
{
    return 2;
}

static UfoTaskMode
ufo_tile_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_REDUCTOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_tile_task_process (UfoTask *task,
                       UfoBuffer **inputs,
                       UfoBuffer *output,
                       UfoRequisition *requisition)
{
    UfoTileTaskPrivate *priv;
    UfoGpuNode *node;
    cl_command_queue cmd_queue;
    cl_mem in_mem;
    
    priv = UFO_TILE_TASK_GET_PRIVATE (task);
    priv->x = 0;
    priv->y = 0;

    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
    UFO_RESOURCES_CHECK_CLERR (clEnqueueCopyBuffer (cmd_queue, in_mem, priv->temp,
                               0, 0, ufo_buffer_get_size (inputs[0]),
                               0, NULL, NULL));

    return FALSE;
}

static gboolean
ufo_tile_task_generate (UfoTask *task,
                        UfoBuffer *output,
                        UfoRequisition *requisition)
{
    UfoTileTaskPrivate *priv;
    UfoGpuNode *node;
    cl_command_queue cmd_queue;
    cl_mem out_mem;
    
    priv = UFO_TILE_TASK_GET_PRIVATE (task);

    if (priv->y == priv->num_vertical)
        return FALSE;

    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);

    const size_t src_origin[3] = { 
        priv->x * requisition->dims[0] * sizeof(float), 
        priv->y * requisition->dims[1],
        0
    };
    const size_t dst_origin[3] = { 0, 0, 0 };
    const size_t region[3] = {
        requisition->dims[0] * sizeof(float),
        requisition->dims[1],
        1
    };

    UFO_RESOURCES_CHECK_CLERR (clEnqueueCopyBufferRect (cmd_queue, priv->temp, out_mem,
                                                        src_origin, dst_origin, region,
                                                        priv->in_width * sizeof(float), 0,
                                                        region[0], 0,
                                                        0, NULL, NULL));

    priv->x++;

    if (priv->x == priv->num_horizontal) {
        priv->x = 0;
        priv->y++;
    }

    return TRUE;
}

static void
ufo_tile_task_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    UfoTileTaskPrivate *priv = UFO_TILE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_WIDTH:
            priv->width = g_value_get_uint (value);
            break;
        case PROP_HEIGHT:
            priv->height = g_value_get_uint (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_tile_task_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    UfoTileTaskPrivate *priv = UFO_TILE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_WIDTH:
            g_value_set_uint (value, priv->width);
            break;
        case PROP_HEIGHT:
            g_value_set_uint (value, priv->height);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_tile_task_finalize (GObject *object)
{
    UfoTileTaskPrivate *priv;
    
    priv = UFO_TILE_TASK_GET_PRIVATE (object);

    if (priv->temp) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->temp));
        priv->temp = NULL;
    }

    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }

    G_OBJECT_CLASS (ufo_tile_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_tile_task_setup;
    iface->get_num_inputs = ufo_tile_task_get_num_inputs;
    iface->get_num_dimensions = ufo_tile_task_get_num_dimensions;
    iface->get_mode = ufo_tile_task_get_mode;
    iface->get_requisition = ufo_tile_task_get_requisition;
    iface->process = ufo_tile_task_process;
    iface->generate = ufo_tile_task_generate;
}

static void
ufo_tile_task_class_init (UfoTileTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_tile_task_set_property;
    oclass->get_property = ufo_tile_task_get_property;
    oclass->finalize = ufo_tile_task_finalize;

    properties[PROP_WIDTH] =
        g_param_spec_uint ("width",
            "Tile width",
            "Tile width whose multiple is the final image width, zero meaning full width",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE);

    properties[PROP_HEIGHT] =
        g_param_spec_uint ("height",
            "Tile height",
            "Tile height whose multiple is the final image height, zero meaning full height",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoTileTaskPrivate));
}

static void
ufo_tile_task_init(UfoTileTask *self)
{
    self->priv = UFO_TILE_TASK_GET_PRIVATE(self);
    self->priv->width = 0;
    self->priv->height = 0;
    self->priv->temp = NULL;
    self->priv->context = NULL;
}
