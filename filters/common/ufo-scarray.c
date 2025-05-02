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

#include "ufo-math.h"
#include "ufo-scarray.h"

struct _UfoScarray {
    GValueArray *array;
};

UfoScarray *
ufo_scarray_new (guint num_elements, GType type, GValue *init_value)
{
    UfoScarray *scarray;
    GValue zero = G_VALUE_INIT;
    GValue value = G_VALUE_INIT;

    if (init_value == NULL) {
        g_value_init (&zero, G_TYPE_INT);
        g_value_init (&value, type);
        g_value_set_int (&zero, 0);
        g_value_transform (&zero, &value);
    } else {
        value = *init_value;
    }

    scarray = g_new0 (UfoScarray, 1);
    scarray->array = g_value_array_new (num_elements);

    for (guint i = 0; i < num_elements; i++)
        g_value_array_append (scarray->array, &value);

    if (init_value == NULL) {
        g_value_unset (&zero);
        g_value_unset (&value);
    }

    return scarray;
}

UfoScarray *
ufo_scarray_copy (const UfoScarray *array)
{
    UfoScarray *cpy = g_new0 (UfoScarray, 1);
    cpy->array = g_value_array_copy (array->array);

    return cpy;
}

void
ufo_scarray_free (UfoScarray *scarray)
{
    g_value_array_free (scarray->array);
    g_free (scarray);
}

void
ufo_scarray_set_value (UfoScarray *scarray,
                       GValue *value)
{
    g_value_set_boxed (value, scarray->array);
}

void
ufo_scarray_get_value (UfoScarray *scarray,
                       const GValue *value)
{
    g_value_array_free (scarray->array);
    scarray->array = g_value_array_copy (g_value_get_boxed (value));
}

void
ufo_scarray_insert (UfoScarray *scarray,
                    guint index,
                    const GValue *value)
{
    g_value_array_insert (scarray->array, index, value);
}

gint
ufo_scarray_get_int (UfoScarray     *scarray,
                     guint           index)
{
    if (scarray->array->n_values == 1)
        return g_value_get_float (g_value_array_get_nth (scarray->array, 0));

    g_assert (scarray->array->n_values > index);
    return g_value_get_int (g_value_array_get_nth (scarray->array, index));
}

gfloat
ufo_scarray_get_float (UfoScarray *scarray,
                       guint index)
{
    if (scarray->array->n_values == 1)
        return g_value_get_float (g_value_array_get_nth (scarray->array, 0));

    g_assert (scarray->array->n_values > index);
    return g_value_get_float (g_value_array_get_nth (scarray->array, index));
}

gdouble
ufo_scarray_get_double (UfoScarray *scarray,
                        guint index)
{
    if (scarray->array->n_values == 1)
        return g_value_get_double (g_value_array_get_nth (scarray->array, 0));

    g_assert (scarray->array->n_values > index);
    return g_value_get_double (g_value_array_get_nth (scarray->array, index));
}

gboolean
ufo_scarray_has_n_values (UfoScarray *scarray,
                          guint num)
{
    return scarray->array->n_values == num;
}

gboolean
ufo_scarray_is_almost_zero (UfoScarray *scarray)
{
    guint i;

    for (i = 0; i < scarray->array->n_values; i++) {
        if (!UFO_MATH_ARE_ALMOST_EQUAL (ufo_scarray_get_double (scarray, i), 0)) {
            return FALSE;
        }
    }

    return TRUE;
}
