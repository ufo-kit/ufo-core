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

#include "ufo-priv.h"
#include "ufo-monitor-task.h"

struct _UfoMonitorTaskPrivate {
    guint n_items;
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoMonitorTask, ufo_monitor_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_MONITOR_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_MONITOR_TASK, UfoMonitorTaskPrivate))

enum {
    PROP_0,
    PROP_NUM_ITEMS,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_monitor_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_MONITOR_TASK, NULL));
}

static void
ufo_monitor_task_setup (UfoTask *task,
                        UfoResources *resources,
                        GError **error)
{
}

static void
ufo_monitor_task_get_requisition (UfoTask *task,
                                  UfoBuffer **inputs,
                                  UfoRequisition *requisition,
                                  GError **error)
{
    ufo_buffer_get_requisition (inputs[0], requisition);
}

static guint
ufo_monitor_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_monitor_task_get_num_dimensions (UfoTask *task,
                                     guint input)
{
    return 2;
}

static UfoTaskMode
ufo_monitor_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_PROCESSOR;
}

static gchar *
join_list (GList *list, const gchar *sep)
{
    gchar **array;
    GList *it;
    gchar *result;
    guint i = 0;

    array = g_new0 (gchar *, g_list_length (list) + 1);

    g_list_for (list, it)
        array[i++] = it->data;

    result = g_strjoinv (sep, array);
    g_free (array);
    return result;
}

static GList *
get_values (UfoBuffer *buffer,
            GList *keys)
{
    GList *result;
    GList *it;

    result = NULL;

    g_list_for (keys, it) {
        GValue *value;
        GValue target = {0,};

        value = ufo_buffer_get_metadata (buffer, (gchar *) it->data);
        g_value_init (&target, G_TYPE_STRING);
        g_value_transform (value, &target);
        result = g_list_append (result, g_value_dup_string (&target));
        g_value_unset (&target);
    }

    return result;
}

static GList *
zip (GList *a, GList *b)
{
    GList *result;
    guint n;

    result = NULL;
    n = MIN (g_list_length (a), g_list_length (b));

    for (guint i = 0; i < n; i++)
        result = g_list_append (result, g_strdup_printf ("%s=%s", (gchar *) g_list_nth_data (a, i), (gchar *) g_list_nth_data (b, i)));

    return result;
}

static gboolean
ufo_monitor_task_process (UfoTask *task,
                          UfoBuffer **inputs,
                          UfoBuffer *output,
                          UfoRequisition *requisition)
{
    UfoMonitorTaskPrivate *priv;
    UfoBufferLocation location;
    UfoBufferLayout layout;
    GList *keys;
    GList *values;
    GList *zipped;
    GList *sizes;
    gchar *kvstring;
    gchar *dimstring;
    gchar *layout_info[] = {"R", "CI"};

    priv = UFO_MONITOR_TASK_GET_PRIVATE (task);
    location = ufo_buffer_get_location (inputs[0]);
    layout = ufo_buffer_get_layout (inputs[0]);
    keys = ufo_buffer_get_metadata_keys (inputs[0]);
    sizes = NULL;

    for (guint i = 0; i < requisition->n_dims; i++)
        sizes = g_list_append (sizes, g_strdup_printf ("%zu", requisition->dims[i]));

    values = get_values (inputs[0], keys);
    zipped = zip (keys, values);
    dimstring = join_list (sizes, " ");
    kvstring = join_list (zipped, ", ");

    g_print ("monitor: dims=[%s] layout=[%s] keys=[%s] location=",
             dimstring, layout_info[layout], kvstring);

    switch (location) {
        case UFO_BUFFER_LOCATION_HOST:
            g_print ("host");
            break;
        case UFO_BUFFER_LOCATION_DEVICE:
            g_print ("device");
            break;
        case UFO_BUFFER_LOCATION_DEVICE_IMAGE:
            g_print ("image");
            break;
        case UFO_BUFFER_LOCATION_INVALID:
            g_print ("invalid");
            break;
    }

    g_print ("\n");

    if (priv->n_items > 0) {
        guint32 *data;

        data = (guint32 *) ufo_buffer_get_host_array (inputs[0], NULL);

        g_print ("  ");

        for (guint i = 0; i < priv->n_items; i++) {
            g_print ("0x%08x ", data[i]);

            if ((i != 0) && (((i + 1) % 8) == 0))
                g_print ("\n  ");
        }

        if ((priv->n_items % 8) != 0)
            g_print ("\n");
    }

    ufo_buffer_swap_data (inputs[0], output);

    g_free (dimstring);
    g_free (kvstring);
    g_list_free (keys);
    g_list_free_full (values, (GDestroyNotify) g_free);
    g_list_free_full (zipped, (GDestroyNotify) g_free);
    g_list_free_full (sizes, (GDestroyNotify) g_free);

    return TRUE;
}
static void
ufo_monitor_task_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
    UfoMonitorTaskPrivate *priv;

    priv = UFO_MONITOR_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_ITEMS:
            priv->n_items = g_value_get_uint (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_monitor_task_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
    UfoMonitorTaskPrivate *priv;

    priv = UFO_MONITOR_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_NUM_ITEMS:
            g_value_set_uint (value, priv->n_items);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_monitor_task_setup;
    iface->get_num_inputs = ufo_monitor_task_get_num_inputs;
    iface->get_num_dimensions = ufo_monitor_task_get_num_dimensions;
    iface->get_mode = ufo_monitor_task_get_mode;
    iface->get_requisition = ufo_monitor_task_get_requisition;
    iface->process = ufo_monitor_task_process;
}

static void
ufo_monitor_task_class_init (UfoMonitorTaskClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);

    oclass->set_property = ufo_monitor_task_set_property;
    oclass->get_property = ufo_monitor_task_get_property;

    properties[PROP_NUM_ITEMS] =
        g_param_spec_uint ("print",
            "Number of items to print",
            "Number of items to print",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE);

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (oclass, i, properties[i]);

    g_type_class_add_private (klass, sizeof(UfoMonitorTaskPrivate));
}

static void
ufo_monitor_task_init(UfoMonitorTask *self)
{
    self->priv = UFO_MONITOR_TASK_GET_PRIVATE (self);
    self->priv->n_items = 0;
}
