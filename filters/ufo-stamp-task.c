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

#include <pango/pangocairo.h>
#include "ufo-stamp-task.h"


struct _UfoStampTaskPrivate {
    PangoFontDescription *font_description;
    PangoLayout *layout;
    cairo_t *layout_context;
    gchar *font;
    gfloat scale;
    guint num;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoStampTask, ufo_stamp_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_STAMP_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_STAMP_TASK, UfoStampTaskPrivate))

enum {
    PROP_0,
    PROP_FONT,
    PROP_SCALE,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static cairo_t *
create_cairo_context (gint width,
                      gint height,
                      cairo_surface_t** surf,
                      guchar** buffer)
{
    *buffer = g_malloc (4 * width * height * sizeof (guchar));
    *surf = cairo_image_surface_create_for_data (*buffer, CAIRO_FORMAT_ARGB32, width, height, 4 * width);
    return cairo_create (*surf);
}

static cairo_t *
create_layout_context ()
{
    cairo_surface_t *temp_surface;
    cairo_t *context;

    temp_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 0, 0);
    context = cairo_create (temp_surface);
    cairo_surface_destroy (temp_surface);
    return context;
}

static void
render_text (UfoStampTaskPrivate *priv,
             const gchar *text,
             gint *width,
             gint *height,
             guchar **data)
{
    cairo_t *render_context;
    cairo_surface_t *surface;

    pango_layout_set_text (priv->layout, text, -1);
    pango_layout_get_size (priv->layout, width, height);

    *width /= PANGO_SCALE;
    *height /= PANGO_SCALE;

    render_context = create_cairo_context (*width, *height, &surface, data);

    cairo_set_source_rgba (render_context, 0, 0, 0, 1);
    cairo_paint (render_context);

    cairo_set_source_rgba (render_context, 1, 1, 1, 1);
    pango_cairo_show_layout (render_context, priv->layout);

    cairo_destroy (render_context);
    cairo_surface_destroy (surface);
}

UfoNode *
ufo_stamp_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_STAMP_TASK, NULL));
}

static void
ufo_stamp_task_setup (UfoTask *task,
                      UfoResources *resources,
                      GError **error)
{
    UfoStampTaskPrivate *priv;

    priv = UFO_STAMP_TASK_GET_PRIVATE (task);

    priv->num = 0;
    priv->font_description = pango_font_description_from_string (priv->font);
    priv->layout_context = create_layout_context ();
    priv->layout = pango_cairo_create_layout (priv->layout_context);
    pango_layout_set_font_description (priv->layout, priv->font_description);
}

static void
ufo_stamp_task_get_requisition (UfoTask *task,
                                UfoBuffer **inputs,
                                UfoRequisition *requisition,
                                GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_stamp_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_stamp_task_get_num_dimensions (UfoTask *task,
                                             guint input)
{
    return 2;
}

static UfoTaskMode
ufo_stamp_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR;
}

static gboolean
ufo_stamp_task_process (UfoTask *task,
                        UfoBuffer **inputs,
                        UfoBuffer *output,
                        UfoRequisition *requisition)
{
    UfoStampTaskPrivate *priv;
    gchar *text;
    gint full_width;
    gint full_height;
    gfloat *in;
    gfloat *out;
    gfloat scale;
    guchar *data = NULL;
    gint width = 0;
    gint height = 0;

    priv = UFO_STAMP_TASK_GET_PRIVATE (task);
    text = g_strdup_printf ("%06i", priv->num);
    render_text (priv, text, &width, &height, &data);

    in = ufo_buffer_get_host_array (inputs[0], NULL);
    out = ufo_buffer_get_host_array (output, NULL);
    full_width = (gint) requisition->dims[0];
    full_height = (gint) requisition->dims[1];

    scale = priv->scale / 255.0f;

    for (gint y = 0; y < full_height; y++) {
        for (gint x = 0; x < full_width; x++) {
            if (x < width && y < height)
                out[y * full_width + x] = in[y * full_width + x] + data[y * width * 4 + 4 * x] * scale;
            else
                out[y * full_width + x] = in[y * full_width + x];
        }
    }

    g_free (data);
    g_free (text);
    priv->num++;
    return TRUE;
}

static void
ufo_stamp_task_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    UfoStampTaskPrivate *priv = UFO_STAMP_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_FONT:
            g_free (priv->font);
            priv->font = g_strdup (g_value_get_string (value));
            break;
        case PROP_SCALE:
            priv->scale = g_value_get_float (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_stamp_task_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    UfoStampTaskPrivate *priv = UFO_STAMP_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_FONT:
            g_value_set_string (value, priv->font);
            break;
        case PROP_SCALE:
            g_value_set_float (value, priv->scale);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_stamp_task_dispose (GObject *object)
{
    UfoStampTaskPrivate *priv;

    priv = UFO_STAMP_TASK_GET_PRIVATE (object);
    g_object_unref (priv->layout);
    G_OBJECT_CLASS (ufo_stamp_task_parent_class)->dispose (object);
}

static void
ufo_stamp_task_finalize (GObject *object)
{
    UfoStampTaskPrivate *priv;

    priv = UFO_STAMP_TASK_GET_PRIVATE (object);
    pango_font_description_free (priv->font_description);
    cairo_destroy (priv->layout_context);

    G_OBJECT_CLASS (ufo_stamp_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_stamp_task_setup;
    iface->get_num_inputs = ufo_stamp_task_get_num_inputs;
    iface->get_num_dimensions = ufo_stamp_task_get_num_dimensions;
    iface->get_mode = ufo_stamp_task_get_mode;
    iface->get_requisition = ufo_stamp_task_get_requisition;
    iface->process = ufo_stamp_task_process;
}

static void
ufo_stamp_task_class_init (UfoStampTaskClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_stamp_task_set_property;
    oclass->get_property = ufo_stamp_task_get_property;
    oclass->dispose = ufo_stamp_task_dispose;
    oclass->finalize = ufo_stamp_task_finalize;

    properties[PROP_FONT] =
        g_param_spec_string ("font",
            "Pango font name",
            "Pango font name",
            "Mono 9",
            G_PARAM_READWRITE);

    properties[PROP_SCALE] =
        g_param_spec_float ("scale",
            "Scale factor",
            "Scale factor to change brightness of text",
            -G_MAXFLOAT, G_MAXFLOAT, 1.0f,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (oclass, sizeof (UfoStampTaskPrivate));
}

static void
ufo_stamp_task_init(UfoStampTask *self)
{
    self->priv = UFO_STAMP_TASK_GET_PRIVATE(self);
    self->priv->font = g_strdup ("Mono 9");
    self->priv->scale = 1.0f;
}
