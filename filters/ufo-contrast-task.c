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
 *
 * Authored by: Alexandre Lewkowicz (lewkow_a@epita.fr)
 */
#include "config.h"

#include <math.h>
#include <stdlib.h>

#include "ufo-contrast-task.h"

struct _UfoContrastTaskPrivate {
    gboolean remove_high;
};

typedef struct _UfoHistogram {
    float max;
    float min;
    double median;
    unsigned num_bins;
    /* value of pic in histogram */
    double pic;
    double step;
    /* array of bins if num_bins size */
    unsigned *bins;
} UfoHistogram;

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoContrastTask, ufo_contrast_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_CONTRAST_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CONTRAST_TASK, UfoContrastTaskPrivate))

enum {
    PROP_0,
    PROP_REMOVE_HIGH,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_contrast_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_CONTRAST_TASK, NULL));
}

static void
ufo_contrast_task_setup (UfoTask *task,
                       UfoResources *resources,
                       GError **error)
{
}

static void
ufo_contrast_task_get_requisition (UfoTask *task,
                                   UfoBuffer **inputs,
                                   UfoRequisition *requisition,
                                   GError **error)
{
    ufo_buffer_get_requisition(inputs[0], requisition);
}

static guint
ufo_contrast_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_contrast_task_get_num_dimensions (UfoTask *task,
                                      guint input)
{
    return 2;
}

static UfoTaskMode
ufo_contrast_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR | UFO_TASK_MODE_CPU;
}

static UfoHistogram *
new_histogram(UfoBuffer *ufo_input)
{
    UfoRequisition input_req;
    ufo_buffer_get_requisition(ufo_input, &input_req);
    double num_elt = (double) input_req.dims[0] * (double) input_req.dims[1];
    unsigned num_bins = (unsigned) sqrt(num_elt);
    float *input = ufo_buffer_get_host_array(ufo_input, NULL);

    UfoHistogram *res;

    res = g_new0 (UfoHistogram, 1);
    res->bins = g_new0 (unsigned, num_bins);
    res->num_bins = num_bins;
    res->min = input[0];
    res->max = input[0];

    /* Compute max and min element */
    for (unsigned j = 0; j < input_req.dims[1]; ++j) {
        for (unsigned i = 0; i < input_req.dims[0]; ++i) {
            if (res->max < input[i + j * input_req.dims[0]])
                res->max = input[i + j * input_req.dims[0]];

            if (res->min > input[i + j * input_req.dims[0]])
                res->min = input[i + j * input_req.dims[0]];
        }
    }

    /* Compute histogram */
    res->step = (double) (res->max - res->min) / (num_bins - 1);

    for (unsigned j = 0; j < input_req.dims[1]; ++j) {
        for (unsigned i = 0; i < input_req.dims[0]; ++i) {
            unsigned idx = (unsigned) round((input[i + j * input_req.dims[0]] - res->min) / res->step);
            ++res->bins[idx];
        }
    }

    /* Compute median and pic value */
    unsigned nb_elt_seen = 0;
    unsigned max_elt_count_idx = 0;

    for (unsigned i = 0; i < num_bins; ++i) {
        nb_elt_seen += res->bins[i];

        /* Once median value is set, don't set it again */
        if (res->median == 0 &&
            nb_elt_seen >= (input_req.dims[0] * input_req.dims[1]) / 2)
            res->median = i * res->step + res->min;

        if (res->bins[max_elt_count_idx] < res->bins[i])
            max_elt_count_idx = i;
    }

    res->pic = max_elt_count_idx * res->step + res->min;
    return res;
}

/* search [start, end[ interval, return index with highest value */
static double
histogram_get_pic (UfoHistogram *histogram, unsigned start, unsigned end)
{
    unsigned res = start;

    for (unsigned i = start + 1; i < end; ++i) {
        if (histogram->bins[i] > histogram->bins[res])
            res = i;
    }

    return res * histogram->step + histogram->min;
}

/* Rescale image from low -> high to 0 -> 1 values and enhance constrast by
 * gamma. Gamma == 1 makes a linear mapping. gamma < 1 produces brighter image.
 * gamma > 1 produces darker image */
static void
imadjust (UfoBuffer *ufo_src, UfoBuffer *ufo_dst, double low, double high,
          double gamma, float new_high)
{
    UfoRequisition req;
    ufo_buffer_get_requisition (ufo_src, &req);
    float *src = ufo_buffer_get_host_array(ufo_src, NULL);
    float *dst = ufo_buffer_get_host_array(ufo_dst, NULL);

    for (unsigned j = 0; j < req.dims[1]; ++j) {
        for (unsigned i = 0; i < req.dims[0]; ++i) {
            if (src[i + j * req.dims[0]] >= high)
                dst[i + j * req.dims[0]] = new_high;
            else if (src[i + j * req.dims[0]] <= low)
                dst[i + j * req.dims[0]] = 0.0;
            else {
                double normalized = (src[i + j * req.dims[0]] - low) / (double) (high - low);
                double val = pow(normalized, gamma);
                dst[i + j * req.dims[0]] = (float) val;
            }
        }
    }
}

static gboolean
ufo_contrast_task_process (UfoTask *task,
                           UfoBuffer **inputs,
                           UfoBuffer *output,
                           UfoRequisition *requisition)
{
    UfoContrastTaskPrivate *priv = UFO_CONTRAST_TASK_GET_PRIVATE (task);
    UfoRequisition input_req;
    ufo_buffer_get_requisition (inputs[0], &input_req);
    UfoHistogram *histogram = new_histogram (inputs[0]);
    /* gamma < 1 to make image more bright and enhance contrast */
    double gamma = 0.3;

    /* Remove values under pic, enhance contrast and normalize image */
    double pic = histogram_get_pic(histogram, 1, histogram->num_bins - 1);

    /* Remove high pixels of output */
    if (priv->remove_high) {
        double crop_max = histogram->max - (histogram->max - pic) / 2;
        imadjust (inputs[0], output, pic,  crop_max, gamma, 0.0);
    }
    else {
        /* transpose image from [pic, 1] to [0, 1] */
        imadjust (inputs[0], output, pic,  histogram->max, gamma, 1.0);
    }

    g_free (histogram->bins);
    g_free (histogram);
    return TRUE;
}

static void
ufo_contrast_task_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    UfoContrastTaskPrivate *priv = UFO_CONTRAST_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_REMOVE_HIGH:
            priv->remove_high = g_value_get_boolean(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_contrast_task_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    UfoContrastTaskPrivate *priv = UFO_CONTRAST_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_REMOVE_HIGH:
            g_value_set_boolean(value, priv->remove_high);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_contrast_task_finalize (GObject *object)
{
    G_OBJECT_CLASS (ufo_contrast_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_contrast_task_setup;
    iface->get_num_inputs = ufo_contrast_task_get_num_inputs;
    iface->get_num_dimensions = ufo_contrast_task_get_num_dimensions;
    iface->get_mode = ufo_contrast_task_get_mode;
    iface->get_requisition = ufo_contrast_task_get_requisition;
    iface->process = ufo_contrast_task_process;
}

static void
ufo_contrast_task_class_init (UfoContrastTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_contrast_task_set_property;
    gobject_class->get_property = ufo_contrast_task_get_property;
    gobject_class->finalize = ufo_contrast_task_finalize;

    properties[PROP_REMOVE_HIGH] =
        g_param_spec_boolean ("remove-high",
            "Tells whether or not to set high intensity pixels to 0",
            "Tells whether or not to set high intensity pixels to 0",
            0,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoContrastTaskPrivate));
}

static void
ufo_contrast_task_init(UfoContrastTask *self)
{
    self->priv = UFO_CONTRAST_TASK_GET_PRIVATE(self);
    self->priv->remove_high = 0;
}
