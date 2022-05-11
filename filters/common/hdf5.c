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

#include "common/hdf5.h"

gboolean
ufo_hdf5_can_open (const gchar *filename)
{
    gchar *delimiter;

    delimiter = g_strrstr (filename, ":");

    if (delimiter == NULL)
        return FALSE;

    /* delimiter is not preceeded by three characters */
    if (((delimiter > filename + 3) && !g_str_has_prefix (delimiter - 3, ".h5")) &&
        ((delimiter > filename + 4) && !g_str_has_prefix (delimiter - 4, ".nxs")) &&
        ((delimiter > filename + 5) && !g_str_has_prefix (delimiter - 5, ".hdf5")))
        return FALSE;

    /* no dataset after delimiter */
    if ((delimiter[1] == '\0') || (delimiter[2] == '\0'))
        return FALSE;

    return TRUE;
}
