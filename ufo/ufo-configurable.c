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

#include <ufo/ufo-configurable.h>
#include <ufo/ufo-config.h>

/**
 * SECTION:ufo-configurable
 * @Short_description: An interface for configurable objects
 * @Title: UfoConfigurable
 *
 * An object that implements the #UfoConfigurable interface provides the common
 * #UfoConfigurable:config property.
 */

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
    config_spec = g_param_spec_object ("config",
                                       "A UfoConfig object",
                                       "A UfoConfig object",
                                       UFO_TYPE_CONFIG,
                                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    g_object_interface_install_property (iface, config_spec);
}
