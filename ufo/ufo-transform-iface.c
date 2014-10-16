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

#include "ufo-transform-iface.h"

/**
 * SECTION: ufo-transform-iface
 * @Short_description: This interface describes the transformation of the data
 * such as wavelet transform, etc.
 * @Title: UfoTransform
 *
 */

typedef UfoTransformIface UfoTransformInterface;

G_DEFINE_INTERFACE (UfoTransform, ufo_transform, G_TYPE_OBJECT)

gboolean
ufo_transform_direct (UfoTransform *transform,
                      UfoBuffer    *input,
                      UfoBuffer    *output,
                      gpointer     pevent)
{
    g_return_val_if_fail(UFO_IS_TRANSFORM (transform) &&
                         UFO_IS_BUFFER (input) &&
                         UFO_IS_BUFFER (output),
                         FALSE);
    return UFO_TRANSFORM_GET_IFACE (transform)->direct (transform, input, output, pevent);
}

gboolean
ufo_transform_inverse (UfoTransform *transform,
                       UfoBuffer    *input,
                       UfoBuffer    *output,
                       gpointer     pevent)
{
    g_return_val_if_fail(UFO_IS_TRANSFORM (transform) &&
                         UFO_IS_BUFFER (input) &&
                         UFO_IS_BUFFER (output),
                         FALSE);
    return UFO_TRANSFORM_GET_IFACE (transform)->inverse (transform, input, output, pevent);
}

static gboolean
ufo_transform_direct_real (UfoTransform *transform,
                           UfoBuffer    *input,
                           UfoBuffer    *output,
                           gpointer     pevent)
{
    g_warning ("%s: `direct' not implemented", G_OBJECT_TYPE_NAME (transform));
    return FALSE;
}

static gboolean
ufo_transform_inverse_real (UfoTransform *transform,
                            UfoBuffer    *input,
                            UfoBuffer    *output,
                            gpointer     pevent)
{
    g_warning ("%s: `inverse' not implemented", G_OBJECT_TYPE_NAME (transform));
    return FALSE;
}

static void
ufo_transform_default_init (UfoTransformInterface *iface)
{
    iface->direct = ufo_transform_direct_real;
    iface->inverse = ufo_transform_inverse_real;
}
