/**
 * SECTION:ufo-filter-splitter
 * @Short_description: Splitter filters control diverging data flows
 * @Title: UfoFilterSplitter
 */

#include <glib.h>
#include <gmodule.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "config.h"
#include "ufo-filter-splitter.h"

G_DEFINE_TYPE (UfoFilterSplitter, ufo_filter_splitter, UFO_TYPE_FILTER)

#define UFO_FILTER_SPLITTER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_SPLITTER, UfoFilterSplitterPrivate))

struct _UfoFilterSplitterPrivate {
    guint  n_splits;
    guint *splits;
};

enum {
    PROP_0,
    PROP_NUM_SPLITS,
    N_PROPERTIES
};

static GParamSpec *splitter_properties[N_PROPERTIES] = { NULL, };

static void
ufo_filter_splitter_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
    UfoFilterSplitterPrivate *priv = UFO_FILTER_SPLITTER_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_SPLITS:
            priv->n_splits = g_value_get_uint (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_filter_splitter_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
    UfoFilterSplitterPrivate *priv = UFO_FILTER_SPLITTER_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_SPLITS:
            g_value_set_uint (value, priv->n_splits);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_filter_splitter_constructed (GObject *object)
{
    UfoInputParameter input_param;
    UfoOutputParameter *output_params;
    UfoFilterSplitter *splitter;
    UfoFilterSplitterPrivate *priv;
    guint n_outputs;

    splitter = UFO_FILTER_SPLITTER (object);
    priv = splitter->priv;

    input_param.n_dims = 2;
    input_param.n_expected_items = UFO_FILTER_INFINITE_INPUT;
    ufo_filter_register_inputs (UFO_FILTER (splitter), 1, &input_param);

    n_outputs = priv->n_splits + 1;
    output_params = g_new0 (UfoOutputParameter, n_outputs);

    for (guint i = 0; i < n_outputs; i++)
        output_params[i].n_dims = 2;

    ufo_filter_register_outputs (UFO_FILTER (splitter), n_outputs, output_params);
    g_free (output_params);

    if (G_OBJECT_CLASS (ufo_filter_splitter_parent_class)->constructed != NULL)
        G_OBJECT_CLASS (ufo_filter_splitter_parent_class)->constructed (object);
}

static void
ufo_filter_splitter_class_init (UfoFilterSplitterClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);
    oclass->set_property = ufo_filter_splitter_set_property;
    oclass->get_property = ufo_filter_splitter_get_property;
    oclass->constructed  = ufo_filter_splitter_constructed;

    splitter_properties[PROP_NUM_SPLITS] =
        g_param_spec_uint ("num-splits",
                           "Number of splits",
                           "Number of splits",
                           0, 256, 0,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_property (oclass, PROP_NUM_SPLITS, splitter_properties[PROP_NUM_SPLITS]);

    g_type_class_add_private (klass, sizeof (UfoFilterSplitterPrivate));
}

static void
ufo_filter_splitter_init (UfoFilterSplitter *self)
{
    self->priv = UFO_FILTER_SPLITTER_GET_PRIVATE (self);

    self->priv->n_splits = 0;
}
