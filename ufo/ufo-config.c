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

#include "config.h"
#include <ufo/ufo-config.h>
#include <ufo/ufo-profiler.h>
#include <ufo/ufo-enums.h>

/**
 * SECTION:ufo-config
 * @Short_description: Access run-time specific settings
 * @Title: UfoConfig
 *
 * A #UfoConfig object is used to keep settings that affect the run-time
 * rather than the parameters of the filter graph. Each object that implements
 * the #UfoConfigurable interface can receive a #UfoConfig object and use
 * the information stored in it.
 */

G_DEFINE_TYPE(UfoConfig, ufo_config, G_TYPE_OBJECT)

#define UFO_CONFIG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_CONFIG, UfoConfigPrivate))

static void add_path (const gchar *path, UfoConfigPrivate *priv);

enum {
    PROP_0,
    PROP_PATHS,
    PROP_DEVICE_TYPE,
    PROP_DISABLE_GPU,
    N_PROPERTIES
};

struct _UfoConfigPrivate {
    GValueArray     *path_array;
    UfoDeviceType    device_type;
    gboolean         disable_gpu;
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

/**
 * ufo_config_new:
 *
 * Create a config object.
 *
 * Return value: A new config object.
 */
UfoConfig *
ufo_config_new (void)
{
    return UFO_CONFIG (g_object_new (UFO_TYPE_CONFIG, NULL));
}

/**
 * ufo_config_get_paths:
 * @config: A #UfoConfig object
 *
 * Get an array of path strings.
 *
 * Returns: (transfer full) (element-type utf8): A list of strings containing
 * file system paths. Use g_list_free() to free it.
 */
GList *
ufo_config_get_paths (UfoConfig *config)
{
    GValueArray *path_array;
    GList *paths;
    guint n_paths;

    g_return_val_if_fail (UFO_IS_CONFIG (config), NULL);

    path_array = config->priv->path_array;
    n_paths = path_array->n_values;
    paths = NULL;

    for (guint i = 0; i < n_paths; i++) {
        paths = g_list_append (paths,
                               g_strdup (g_value_get_string (g_value_array_get_nth (path_array, i))));
    }

    return paths;
}

UfoDeviceType
ufo_config_get_device_type (UfoConfig *config)
{
    g_return_val_if_fail (UFO_IS_CONFIG (config), 0);
    return config->priv->device_type;
}

/**
 * ufo_config_add_paths:
 * @config: A #UfoConfig object
 * @paths: (element-type utf8): List of strings
 *
 * Add @paths to the list of search paths for plugins and OpenCL kernel files.
 */
void
ufo_config_add_paths (UfoConfig *config,
                      GList *paths)
{
    g_return_if_fail (UFO_IS_CONFIG (config));
    g_list_foreach (paths, (GFunc) add_path, config->priv);
}

static void
add_path (const gchar *path,
          UfoConfigPrivate *priv)
{
    GValue path_value = {0};

    g_value_init (&path_value, G_TYPE_STRING);
    g_value_set_string (&path_value, path);
    g_value_array_prepend (priv->path_array, &path_value);
    g_value_unset (&path_value);
}

static void
ufo_config_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
    UfoConfigPrivate *priv = UFO_CONFIG_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_PATHS:
            {
                GValueArray *more_paths;

                more_paths = g_value_get_boxed (value);

                if (more_paths != NULL) {
                    for (guint i = 0; i < more_paths->n_values; i++) {
                        g_value_array_append (priv->path_array,
                                              g_value_array_get_nth (more_paths, i));
                    }
                }
            }
            break;

        case PROP_DEVICE_TYPE:
            priv->device_type = g_value_get_flags (value);
            break;

        case PROP_DISABLE_GPU:
            priv->disable_gpu= g_value_get_boolean (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_config_get_property (GObject      *object,
                         guint         property_id,
                         GValue       *value,
                         GParamSpec   *pspec)
{
    UfoConfigPrivate *priv = UFO_CONFIG_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_PATHS:
            g_value_set_boxed (value, priv->path_array);
            break;

        case PROP_DEVICE_TYPE:
            g_value_set_flags (value, priv->device_type);
            break;

        case PROP_DISABLE_GPU:
            g_value_set_boolean (value, priv->disable_gpu);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_config_dispose (GObject *object)
{
    G_OBJECT_CLASS (ufo_config_parent_class)->finalize (object);
    g_debug ("UfoConfig: disposed");
}

static void
ufo_config_finalize (GObject *object)
{
    UfoConfigPrivate *priv = UFO_CONFIG_GET_PRIVATE (object);

    g_value_array_free (priv->path_array);

    G_OBJECT_CLASS (ufo_config_parent_class)->finalize (object);
    g_debug ("UfoConfig: finalized");
}

static void
ufo_config_class_init (UfoConfigClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);
    oclass->set_property = ufo_config_set_property;
    oclass->get_property = ufo_config_get_property;
    oclass->dispose      = ufo_config_dispose;
    oclass->finalize     = ufo_config_finalize;

    /**
     * UfoConfig:paths:
     *
     * An array of strings with paths pointing to possible filter and kernel
     * file locations.
     */
    properties[PROP_PATHS] =
        g_param_spec_value_array ("paths",
                                  "Array with paths",
                                  "Array with paths",
                                  g_param_spec_string ("path",
                                                       "A path",
                                                       "A path pointing to a filter or kernel",
                                                       ".",
                                                       G_PARAM_READWRITE),
                                  G_PARAM_READWRITE);

    /**
     * UfoConfig:device-class:
     *
     * Let the user select which device class to use for execution.
     *
     * See: #UfoDeviceType for the device classes.
     */
    properties[PROP_DEVICE_TYPE] =
        g_param_spec_flags ("device-type",
                            "Device type to use",
                            "Device type to use",
                            UFO_TYPE_DEVICE_TYPE,
                            UFO_DEVICE_ALL,
                            G_PARAM_READWRITE);
    /**
     * UfoConfig:disable-gpu
     *
     * Let the user select whether the main machine is used for gpu computing.
     *
     */
    properties[PROP_DISABLE_GPU] =
        g_param_spec_boolean ("disable-gpu",
                              "Don't use local machine for GPU computing",
                              "Don't use local machine for GPU computing",
                              FALSE,
                              G_PARAM_READWRITE);

    g_object_class_install_property (oclass, PROP_PATHS,
                                     properties[PROP_PATHS]);
    g_object_class_install_property (oclass, PROP_DEVICE_TYPE,
                                     properties[PROP_DEVICE_TYPE]);
    g_object_class_install_property (oclass, PROP_DISABLE_GPU,
                                     properties[PROP_DISABLE_GPU]);

    g_type_class_add_private(klass, sizeof (UfoConfigPrivate));
}

static void
ufo_config_init (UfoConfig *config)
{
    UfoConfigPrivate *priv;

    config->priv = priv = UFO_CONFIG_GET_PRIVATE (config);
    priv->path_array = g_value_array_new (0);
    priv->device_type = UFO_DEVICE_ALL;

    add_path ("/usr/local/lib64/ufo", priv);
    add_path ("/usr/local/lib/ufo", priv);
    add_path ("/usr/lib64/ufo", priv);
    add_path ("/usr/lib/ufo", priv);
    add_path (UFO_PLUGIN_DIR, priv);
}
