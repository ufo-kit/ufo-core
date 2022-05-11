/*
 * Copyright (C) 2017 Karlsruhe Institute of Technology
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

#ifndef UFO_SCARRAY_H
#define UFO_SCARRAY_H

#include <ufo/ufo.h>

typedef struct _UfoScarray UfoScarray;

UfoScarray *ufo_scarray_new             (guint           num_elements,
                                         GType           type,
                                         GValue         *init_value);
UfoScarray *ufo_scarray_copy            (const UfoScarray *scarray);
void        ufo_scarray_free            (UfoScarray     *scarray);
void        ufo_scarray_set_value       (UfoScarray     *scarray,
                                         GValue         *value);
void        ufo_scarray_get_value       (UfoScarray     *scarray,
                                         const GValue   *value);
void        ufo_scarray_insert          (UfoScarray     *scarray,
                                         guint           index,
                                         const GValue   *value);
gint        ufo_scarray_get_int         (UfoScarray     *scarray,
                                         guint           index);
gfloat      ufo_scarray_get_float       (UfoScarray     *scarray,
                                         guint           index);
gdouble     ufo_scarray_get_double      (UfoScarray     *scarray,
                                         guint           index);
gboolean    ufo_scarray_has_n_values    (UfoScarray     *scarray,
                                         guint           num);
gboolean    ufo_scarray_is_almost_zero  (UfoScarray     *scarray);

#endif
