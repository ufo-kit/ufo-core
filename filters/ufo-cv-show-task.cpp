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

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <opencv2/highgui/highgui.hpp>
#include "ufo-cv-show-task.h"
#include "writers/ufo-writer.h"


struct _UfoCvShowTaskPrivate {
    gfloat min;
    gfloat max;
    gchar *name;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoCvShowTask, ufo_cv_show_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_CV_SHOW_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CV_SHOW_TASK, UfoCvShowTaskPrivate))

enum {
    PROP_0,
    PROP_MINIMUM,
    PROP_MAXIMUM,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_cv_show_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_CV_SHOW_TASK, NULL));
}

static void
ufo_cv_show_task_setup (UfoTask *task,
                        UfoResources *resources,
                        GError **error)
{
}

static void
ufo_cv_show_task_get_requisition (UfoTask *task,
                                  UfoBuffer **inputs,
                                  UfoRequisition *requisition,
                                  GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_cv_show_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_cv_show_task_get_num_dimensions (UfoTask *task,
                                     guint input)
{
    return 2;
}

static UfoTaskMode
ufo_cv_show_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR;
}

static gboolean
ufo_cv_show_task_process (UfoTask *task,
                          UfoBuffer **inputs,
                          UfoBuffer *output,
                          UfoRequisition *requisition)
{
    UfoCvShowTaskPrivate *priv;
    UfoWriterImage image;
    GValue *channels;
    guint width;

    priv = UFO_CV_SHOW_TASK_GET_PRIVATE (task);
    ufo_buffer_copy (inputs[0], output);
    channels = ufo_buffer_get_metadata (inputs[0], "channels");

    /* 
     * We do the conversion ourselves because OpenCV would just map [0.0, 1.0]
     * to [0, 255].
     */
    image.depth = UFO_BUFFER_DEPTH_8U;
    image.requisition = requisition;
    image.min = priv->min;
    image.max = priv->max;
    image.data = (guint8 *) ufo_buffer_get_host_array (inputs[0], NULL);

    ufo_writer_convert_inplace (&image);

    /* FIXME: we assume to have three channels */
    width = channels ? requisition->dims[0] / g_value_get_uint (channels) : requisition->dims[0];
    cv::Mat frame (requisition->dims[1], width, channels ? CV_8UC3 : CV_8UC1, image.data);
    cv::imshow (priv->name, frame);
    cv::waitKey (1);

    return TRUE;
}

static void
ufo_cv_show_task_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
    UfoCvShowTaskPrivate *priv = UFO_CV_SHOW_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_MAXIMUM:
            priv->max = g_value_get_float (value);
            break;
        case PROP_MINIMUM:
            priv->min = g_value_get_float (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_cv_show_task_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
    UfoCvShowTaskPrivate *priv = UFO_CV_SHOW_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_MAXIMUM:
            g_value_set_float (value, priv->max);
            break;
        case PROP_MINIMUM:
            g_value_set_float (value, priv->min);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_cv_show_task_finalize (GObject *object)
{
    g_free (UFO_CV_SHOW_TASK_GET_PRIVATE (object)->name);
    G_OBJECT_CLASS (ufo_cv_show_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_cv_show_task_setup;
    iface->get_num_inputs = ufo_cv_show_task_get_num_inputs;
    iface->get_num_dimensions = ufo_cv_show_task_get_num_dimensions;
    iface->get_mode = ufo_cv_show_task_get_mode;
    iface->get_requisition = ufo_cv_show_task_get_requisition;
    iface->process = ufo_cv_show_task_process;
}

static void
ufo_cv_show_task_class_init (UfoCvShowTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_cv_show_task_set_property;
    oclass->get_property = ufo_cv_show_task_get_property;
    oclass->finalize = ufo_cv_show_task_finalize;

    properties[PROP_MINIMUM] =
        g_param_spec_float ("min",
            "Minimum value for data conversion",
            "Minimum value for data conversion",
            -G_MAXFLOAT, +G_MAXFLOAT, +G_MAXFLOAT,
            (GParamFlags) G_PARAM_READWRITE);

    properties[PROP_MAXIMUM] =
        g_param_spec_float ("max",
            "Minimum value for data conversion",
            "Minimum value for data conversion",
            -G_MAXFLOAT, +G_MAXFLOAT, -G_MAXFLOAT,
            (GParamFlags) G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof(UfoCvShowTaskPrivate));
}

static void
ufo_cv_show_task_init(UfoCvShowTask *self)
{
    self->priv = UFO_CV_SHOW_TASK_GET_PRIVATE(self);

    /* 
     * Set to extreme ends, so that we look for the extrema if not set by the
     * user.
     */
    self->priv->min = +G_MAXFLOAT;
    self->priv->max = -G_MAXFLOAT;
    self->priv->name = g_strdup_printf ("cvshow-%p", self);
}
