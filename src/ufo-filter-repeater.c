/**
 * SECTION:ufo-filter-repeater
 * @Short_description: Repeater filters control diverging data flows
 * @Title: UfoFilterRepeater
 */

#include <glib.h>
#include <gmodule.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "config.h"
#include "ufo-filter-repeater.h"

G_DEFINE_TYPE (UfoFilterRepeater, ufo_filter_repeater, UFO_TYPE_FILTER)

#define UFO_FILTER_REPEATER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_FILTER_REPEATER, UfoFilterRepeaterPrivate))

struct _UfoFilterRepeaterPrivate {
    guint count;
};

enum {
    PROP_0,
    PROP_COUNT,
    N_PROPERTIES
};

static GParamSpec *repeater_properties[N_PROPERTIES] = { NULL, };

static void
ufo_filter_repeater_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
    UfoFilterRepeaterPrivate *priv = UFO_FILTER_REPEATER_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_COUNT:
            priv->count = g_value_get_uint (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_filter_repeater_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
    UfoFilterRepeaterPrivate *priv = UFO_FILTER_REPEATER_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_COUNT:
            g_value_set_uint (value, priv->count);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_filter_repeater_class_init (UfoFilterRepeaterClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);
    oclass->set_property = ufo_filter_repeater_set_property;
    oclass->get_property = ufo_filter_repeater_get_property;

    repeater_properties[PROP_COUNT] =
        g_param_spec_uint ("count",
                           "Number of repetitions",
                           "Number of repetitions",
                           1, G_MAXUINT, 1,
                           G_PARAM_READWRITE);

    g_object_class_install_property (oclass, PROP_COUNT, repeater_properties[PROP_COUNT]);
    g_type_class_add_private (klass, sizeof (UfoFilterRepeaterPrivate));
}

static void
ufo_filter_repeater_init (UfoFilterRepeater *self)
{
    UfoInputParameter input_param;
    UfoOutputParameter output_params;

    self->priv = UFO_FILTER_REPEATER_GET_PRIVATE (self);
    self->priv->count = 1;

    input_param.n_dims = 2;
    input_param.n_expected_items = 1;
    output_params.n_dims = 2;

    ufo_filter_register_inputs (UFO_FILTER (self), 1, &input_param);
    ufo_filter_register_outputs (UFO_FILTER (self), 1, &output_params);
    ufo_filter_set_plugin_name (UFO_FILTER (self), "repeater");
}
