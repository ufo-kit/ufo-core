/**
 * SECTION:ufo-configurable
 * @Short_description: foo
 * @Title: UfoConfigurable
 */

#include "ufo-configurable.h"
#include "ufo-configuration.h"


typedef UfoConfigurableIface UfoConfigurableInterface;
G_DEFINE_INTERFACE (UfoConfigurable, ufo_configurable, G_TYPE_OBJECT)

static void
ufo_configurable_default_init (UfoConfigurableInterface *iface)
{
    GParamSpec *config_spec;

    config_spec = g_param_spec_object ("configuration",
                                       "A UfoConfiguration object",
                                       "A UfoConfiguration object",
                                       UFO_TYPE_CONFIGURATION,
                                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    g_object_interface_install_property (iface, config_spec);
}
