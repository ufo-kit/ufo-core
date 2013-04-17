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

#include <iostream>
#include <viennacl/ocl/backend.hpp>
#include <ufo/ufo-viennacl.h>

/**
 * SECTION:ufo-viennacl
 * @Short_description: Routines to configure viennacl.
 */

void copy_hash_pair_to_std_map (gpointer key, gpointer value, gpointer std_map);

/**
 * ufo_viennacl_setup:
 * @resources: A #UfoResources object.
 *
 * Configures ViennaCL environment to work with Ufo.
 */
void
ufo_viennacl_setup (UfoResources *resources)
{

  static gboolean _vienna_cl_configured = FALSE;
  static guint _vienna_cl_ufo_context_id = 46;

  cl_context context = (cl_context) ufo_resources_get_context (resources);

  if (!_vienna_cl_configured) {

    GList *ufo_devices = ufo_resources_get_devices (resources);
    GHashTable *ufo_queues = ufo_resources_get_mapped_cmd_queues (resources);

    std::vector<cl_device_id> devices;
    for (guint i = 0; i < g_list_length (ufo_devices); ++i)
        devices.push_back ( (cl_device_id) g_list_nth_data (ufo_devices, i) );

    std::map< cl_device_id, std::vector< cl_command_queue > > queues;
    g_hash_table_foreach (ufo_queues, copy_hash_pair_to_std_map, &queues);
  
    viennacl::ocl::setup_context (_vienna_cl_ufo_context_id,
                                  context,
                                  devices,
                                  queues);

    _vienna_cl_configured = TRUE;
  }

  viennacl::ocl::switch_context(_vienna_cl_ufo_context_id);
}

void
copy_hash_pair_to_std_map (gpointer key,
                           gpointer value,
                           gpointer std_map)
{
  typedef std::map< cl_device_id, std::vector< cl_command_queue > > map_type;
  typedef cl_device_id key_type;
  typedef cl_command_queue value_type;
  typedef std::vector<cl_command_queue> vector_type;

  map_type *dst_map = (map_type *) std_map;

  vector_type v;
  v.push_back ( (value_type) value );
  (*dst_map) [ (key_type) key] = v;
}