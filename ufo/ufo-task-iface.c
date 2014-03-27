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

#include <ufo/ufo-task-iface.h>

typedef UfoTaskIface UfoTaskInterface;

G_DEFINE_INTERFACE (UfoTask, ufo_task, G_TYPE_OBJECT)

/**
 * UfoTaskError:
 * @UFO_TASK_ERROR_SETUP: Error during setup of a task.
 */
GQuark
ufo_task_error_quark ()
{
    return g_quark_from_static_string ("ufo-task-error-quark");
}

void
ufo_task_setup (UfoTask *task,
                UfoResources *resources,
                GError **error)
{
    UFO_TASK_GET_IFACE (task)->setup (task, resources, error);
}

void
ufo_task_get_requisition (UfoTask *task,
                          UfoBuffer **inputs,
                          UfoRequisition *requisition)
{
    UFO_TASK_GET_IFACE (task)->get_requisition (task, inputs, requisition);
}

guint
ufo_task_get_num_inputs (UfoTask *task)
{
    return UFO_TASK_GET_IFACE (task)->get_num_inputs (task);
}

guint
ufo_task_get_num_dimensions (UfoTask *task,
                             guint input)
{
    return UFO_TASK_GET_IFACE (task)->get_num_dimensions (task, input);
}

UfoTaskMode
ufo_task_get_mode (UfoTask *task)
{
    return UFO_TASK_GET_IFACE (task)->get_mode (task);
}

void
ufo_task_set_json_object_property (UfoTask *task,
                                   const gchar *prop_name,
                                   JsonObject *object)
{
    UFO_TASK_GET_IFACE (task)->set_json_object_property (task, prop_name, object);
}

gboolean
ufo_task_process (UfoTask *task,
                  UfoBuffer **inputs,
                  UfoBuffer *output,
                  UfoRequisition *requisition)
{
    return UFO_TASK_GET_IFACE (task)->process (task, inputs, output, requisition);
}

gboolean
ufo_task_generate (UfoTask *task,
                   UfoBuffer *output,
                   UfoRequisition *requisition)
{
    return UFO_TASK_GET_IFACE (task)->generate (task, output, requisition);
}

gboolean
ufo_task_uses_gpu (UfoTask *task)
{
    return ufo_task_get_mode (task) & UFO_TASK_MODE_GPU;
}

gboolean
ufo_task_uses_cpu (UfoTask *task)
{
    return ufo_task_get_mode (task) & UFO_TASK_MODE_CPU;
}

static void
ufo_task_set_json_object_property_real (UfoTask *task,
                                        const gchar *prop_name,
                                        JsonObject *object)
{
    g_warning ("`set_json_object_property' not implemented");
}

static void
ufo_task_setup_real (UfoTask *task,
                     UfoResources *resources,
                     GError **error)
{
    g_warning ("`setup' not implemented");
}

static void
ufo_task_get_requisition_real (UfoTask *task,
                               UfoBuffer **inputs,
                               UfoRequisition *requisition)
{
    g_warning ("`get_allocation' not implemented");
}

static guint
ufo_task_get_num_inputs_real (UfoTask *task)
{
    g_warning ("`get_num_inputs' not implemented");
    return 0;
}

static guint
ufo_task_get_num_dimensions_real (UfoTask *task,
                                  guint input)
{
    g_warning ("`get_num_dimensions' not implemented");
    return 0;
}

static UfoTaskMode
ufo_task_get_mode_real (UfoTask *task)
{
    g_warning ("`get_mode' not implemented");
    return 0;
}

static gboolean
ufo_task_process_real (UfoTask *task,
                       UfoBuffer **inputs,
                       UfoBuffer *output,
                       UfoRequisition *requisition)
{
    g_warning ("`process' of UfoTaskInterface not implemented");
    return FALSE;
}

static gboolean
ufo_task_generate_real (UfoTask *task,
                        UfoBuffer *output,
                        UfoRequisition *requisition)
{
    g_warning ("`generate' of UfoTaskInterface not implemented");
    return FALSE;
}


static void
ufo_task_default_init (UfoTaskInterface *iface)
{
    iface->setup = ufo_task_setup_real;
    iface->get_requisition = ufo_task_get_requisition_real;
    iface->get_num_inputs = ufo_task_get_num_inputs_real;
    iface->get_num_dimensions = ufo_task_get_num_dimensions_real;
    iface->get_mode = ufo_task_get_mode_real;
    iface->set_json_object_property = ufo_task_set_json_object_property_real;
    iface->process = ufo_task_process_real;
    iface->generate = ufo_task_generate_real;
}
