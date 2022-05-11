/*
 * Copyright (C) 2015-2016 Karlsruhe Institute of Technology
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

#ifndef UFO_PRIV_H
#define UFO_PRIV_H

#include <glib.h>

G_BEGIN_DECLS

#define g_list_for(list, it) \
        for (it = g_list_first (list); \
             it != NULL; \
             it = g_list_next (it))

guint ceil_power_of_two (guint x);

G_END_DECLS

#endif
