/*
 * Copyright (C) 2015-2017 Karlsruhe Institute of Technology
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
#include "ufo-conebeam.h"

static UfoUniRecoNodeProps node_props[] = {
    {"GENERIC", 8, 0},
    {"GEFORCE_GTX_TITAN", 24, 32},
    {"GEFORCE_GTX_1080_TI", 24, 32},
    {"QUADRO_RTX_8000", 30, 32},
};

GHashTable *
ufo_get_node_props_table (void)
{
    guint i;
    GHashTable *table = g_hash_table_new (g_str_hash, g_str_equal);

    for (i = 0; i < sizeof (node_props) / sizeof (UfoUniRecoNodeProps); i++) {
        g_hash_table_insert (table, node_props[i].name, &node_props[i]);
    }

    return table;
}
