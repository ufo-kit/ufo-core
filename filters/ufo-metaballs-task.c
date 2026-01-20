/*
 * Copyright (C) 2011-2016 Karlsruhe Institute of Technology
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

#include "ufo-metaballs-task.h"

typedef struct {
    gfloat x;
    gfloat y;
    gfloat vx;
    gfloat vy;
    gfloat size;
} Ball;

struct _UfoMetaballsTaskPrivate {
    cl_kernel   kernel;
    cl_mem      balls_mem;

    guint width;
    guint height;
    guint num_balls;
    guint num_iterations;
    guint current_iteration;

    Ball *balls;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoMetaballsTask, ufo_metaballs_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_METABALLS_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_METABALLS_TASK, UfoMetaballsTaskPrivate))

enum {
    PROP_0,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_NUM_BALLS,
    PROP_NUM_ITERATIONS,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_metaballs_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_METABALLS_TASK, NULL));
}

static void
ufo_metaballs_task_setup (UfoTask *task,
                          UfoResources *resources,
                          GError **error)
{
    UfoMetaballsTaskPrivate *priv;
    gfloat f_width;
    gfloat f_height;
    cl_context context;
    cl_int err = CL_SUCCESS;

    priv = UFO_METABALLS_TASK_GET_PRIVATE (task);
    context = ufo_resources_get_context (resources);

    priv->kernel = ufo_resources_get_kernel (resources, "metaballs.cl", "draw_metaballs", NULL, error);

    if (priv->kernel != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);

    priv->current_iteration = 0;
    priv->balls = g_malloc0 (priv->num_balls * sizeof (Ball));

    f_width = (gfloat) priv->width;
    f_height = (gfloat) priv->height;

    for (guint i = 0; i < priv->num_balls; i++) {
        priv->balls[i].size = (gfloat) g_random_double_range (0.01 * f_width, 0.05 * f_width);
        priv->balls[i].x = (gfloat) g_random_double_range (0.0, (double) f_width);
        priv->balls[i].y = (gfloat) g_random_double_range (0.0, (double) f_height);
        priv->balls[i].vx = (gfloat) g_random_double_range (-4.0, 4.0);
        priv->balls[i].vy = (gfloat) g_random_double_range (-4.0, 4.0);
    };

    priv->balls_mem = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                     priv->num_balls * sizeof (Ball), priv->balls, &err);
    UFO_RESOURCES_CHECK_SET_AND_RETURN (err, error);
}

static void
ufo_metaballs_task_get_requisition (UfoTask *task,
                                    UfoBuffer **inputs,
                                    UfoRequisition *requisition,
                                    GError **error)
{
    UfoMetaballsTaskPrivate *priv;

    priv = UFO_METABALLS_TASK_GET_PRIVATE (task);
    requisition->n_dims = 2;
    requisition->dims[0] = priv->width;
    requisition->dims[1] = priv->height;
}

static guint
ufo_metaballs_task_get_num_inputs (UfoTask *task)
{
    return 0;
}

static guint
ufo_metaballs_task_get_num_dimensions (UfoTask *task,
                               guint input)
{
    return 0;
}

static UfoTaskMode
ufo_metaballs_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_GENERATOR | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_metaballs_task_generate (UfoTask *task,
                             UfoBuffer *output,
                             UfoRequisition *requisition)
{
    UfoMetaballsTaskPrivate *priv;
    UfoGpuNode *node;
    UfoProfiler *profiler;
    cl_command_queue cmd_queue;
    cl_mem out_mem;
    
    priv = UFO_METABALLS_TASK_GET_PRIVATE (task);

    if (priv->current_iteration++ >= priv->num_iterations)
        return FALSE;

    node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
    cmd_queue = ufo_gpu_node_get_cmd_queue (node);
    out_mem = ufo_buffer_get_device_array (output, cmd_queue);

    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof(cl_mem), (cl_mem) &out_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &priv->balls_mem));
    UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 2, sizeof (cl_uint), &priv->num_balls));

    profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
    ufo_profiler_call (profiler, cmd_queue, priv->kernel, 2, requisition->dims, NULL);

    /* Update positions and velocities */
    for (guint i = 0; i < priv->num_balls; i++) {
        priv->balls[i].x += priv->balls[i].vx;
        priv->balls[i].y += priv->balls[i].vy;

        if (priv->balls[i].x < 0 || priv->balls[i].x > priv->width)
            priv->balls[i].vx = -priv->balls[i].vx;

        if (priv->balls[i].y < 0 || priv->balls[i].y > priv->height)
            priv->balls[i].vy = -priv->balls[i].vy;
    }

    UFO_RESOURCES_CHECK_CLERR (clEnqueueWriteBuffer (cmd_queue, priv->balls_mem,
                                                     CL_FALSE,
                                                     0, priv->num_balls * sizeof (Ball), priv->balls,
                                                     0, NULL, NULL));

    return TRUE;
}

static void
ufo_metaballs_task_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
    UfoMetaballsTaskPrivate *priv = UFO_METABALLS_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_WIDTH:
            priv->width = g_value_get_uint(value);
            break;
        case PROP_HEIGHT:
            priv->height = g_value_get_uint(value);
            break;
        case PROP_NUM_BALLS:
            priv->num_balls = g_value_get_uint(value);
            break;
        case PROP_NUM_ITERATIONS:
            priv->num_iterations = g_value_get_uint(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_metaballs_task_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
    UfoMetaballsTaskPrivate *priv = UFO_METABALLS_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_WIDTH:
            g_value_set_uint(value, priv->width);
            break;
        case PROP_HEIGHT:
            g_value_set_uint(value, priv->height);
            break;
        case PROP_NUM_BALLS:
            g_value_set_uint(value, priv->num_balls);
            break;
        case PROP_NUM_ITERATIONS:
            g_value_set_uint(value, priv->num_iterations);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_metaballs_task_finalize (GObject *object)
{
    UfoMetaballsTaskPrivate *priv;

    priv = UFO_METABALLS_TASK_GET_PRIVATE (object);

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }

    if (priv->balls_mem) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->balls_mem));
        priv->balls_mem = NULL;
    }

    g_free (priv->balls);

    G_OBJECT_CLASS (ufo_metaballs_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_metaballs_task_setup;
    iface->get_num_inputs = ufo_metaballs_task_get_num_inputs;
    iface->get_num_dimensions = ufo_metaballs_task_get_num_dimensions;
    iface->get_mode = ufo_metaballs_task_get_mode;
    iface->get_requisition = ufo_metaballs_task_get_requisition;
    iface->generate = ufo_metaballs_task_generate;
}

static void
ufo_metaballs_task_class_init (UfoMetaballsTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_metaballs_task_set_property;
    gobject_class->get_property = ufo_metaballs_task_get_property;
    gobject_class->finalize = ufo_metaballs_task_finalize;

    properties[PROP_WIDTH] =
        g_param_spec_uint("width",
            "Width of the output",
            "Width of the output",
            1, 32768, 512,
            G_PARAM_READWRITE);

    properties[PROP_HEIGHT] =
        g_param_spec_uint("height",
            "Height of the output",
            "Height of the output",
            1, 32768, 512,
            G_PARAM_READWRITE);

    properties[PROP_NUM_BALLS] =
        g_param_spec_uint("number-balls",
            "Number of meta balls",
            "Number of meta balls",
            1, 256, 1,
            G_PARAM_READWRITE);

    properties[PROP_NUM_ITERATIONS] =
        g_param_spec_uint("number",
            "Number of iterations",
            "Number of iterations",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoMetaballsTaskPrivate));
}

static void
ufo_metaballs_task_init(UfoMetaballsTask *self)
{
    UfoMetaballsTaskPrivate *priv;

    self->priv = priv = UFO_METABALLS_TASK_GET_PRIVATE(self);
    priv->width = 512;
    priv->height = 512;
    priv->num_balls = 1;
    priv->num_iterations = 1;
}
