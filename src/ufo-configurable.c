/**
 * SECTION:ufo-configurable
 * @Short_description: An interface for configurable objects
 * @Title: UfoConfigurable
 *
 * An object that implements the #UfoConfigurable interface provides the common
 * #UfoConfigurable:configuration property.
 */

#include "ufo-configurable.h"
#include "ufo-configuration.h"


typedef UfoConfigurableIface UfoConfigurableInterface;
G_DEFINE_INTERFACE (UfoConfigurable, ufo_configurable, G_TYPE_OBJECT)

static void
ufo_configurable_default_init (UfoConfigurableInterface *iface)
{
    GParamSpec *config_spec;

    /**
     * UfoConfigurable:configuration:
     *
     * The #UfoConfiguration object that can be passed to all objects that
     * implement the #UfoConfigurable interface.
     */
    config_spec = g_param_spec_object ("configuration",
                                       "A UfoConfiguration object",
                                       "A UfoConfiguration object",
                                       UFO_TYPE_CONFIGURATION,
                                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    g_object_interface_install_property (iface, config_spec);
}
