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

#include "ufo-method-iface.h"

/**
 * SECTION: ufo-method-iface
 * @Short_description: This interface describes a method
 * @Title: UfoMethod
 *
 */

typedef UfoMethodIface UfoMethodInterface;

G_DEFINE_INTERFACE (UfoMethod, ufo_method, G_TYPE_OBJECT)

gboolean
ufo_method_process (UfoMethod *method,
                    UfoBuffer *input,
                    UfoBuffer *output,
                    gpointer  pevent)
{
    g_return_val_if_fail(UFO_IS_METHOD (method) &&
                         UFO_IS_BUFFER (input) &&
                         UFO_IS_BUFFER (output),
                         FALSE);
    return UFO_METHOD_GET_IFACE (method)->process (method, input, output, pevent);
}

static gboolean
ufo_method_process_real (UfoMethod *method,
                         UfoBuffer *input,
                         UfoBuffer *output,
                         gpointer  pevent)
{
    g_warning ("%s: `process' not implemented", G_OBJECT_TYPE_NAME (method));
    return FALSE;
}

static void
ufo_method_default_init (UfoMethodInterface *iface)
{
    iface->process = ufo_method_process_real;
}
