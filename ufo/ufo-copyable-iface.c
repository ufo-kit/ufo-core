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
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#include "ufo-copyable-iface.h"
#define MAX_INHERITANCE_DEPTH 100

typedef UfoCopyableIface UfoCopyableInterface;

G_DEFINE_INTERFACE (UfoCopyable, ufo_copyable, G_TYPE_OBJECT)

/**
 * ufo_copyable_copy:
 * @origin: A source object that implements #UfoCopyable interface.
 * @copy: A destination object that implements #UfoCopyable interface.
 *
 * Returns: (transfer full): A copy of the @origin object.
 */
UfoCopyable * ufo_copyable_copy (gpointer origin,
                                 gpointer copy)
{
    if (!origin) return NULL;
    g_return_val_if_fail(UFO_IS_COPYABLE (origin), NULL);

    // call top-level interface implementation
    UfoCopyable *_copy = UFO_COPYABLE_GET_IFACE (origin)->copy (origin, copy);
    UfoCopyableIface *current_iface = G_TYPE_INSTANCE_GET_INTERFACE(origin, UFO_TYPE_COPYABLE, UfoCopyableIface);
    UfoCopyableIface *parent_iface  = g_type_interface_peek_parent(current_iface);

    // call parental interface functions
    int nested_level = 0;
    while (parent_iface && nested_level < MAX_INHERITANCE_DEPTH) {
        current_iface = parent_iface;
        _copy = parent_iface->copy (origin, copy);
        parent_iface  = g_type_interface_peek_parent(current_iface);
    }

    return _copy;
}

static UfoCopyable *
ufo_copyable_copy_real (gpointer origin,
                        gpointer copy)
{
    g_warning ("%s: `copy' not implemented", G_OBJECT_TYPE_NAME (origin));
    return NULL;
}

static void
ufo_copyable_default_init (UfoCopyableInterface *iface)
{
    iface->copy = ufo_copyable_copy_real;
}
