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

#include <ufo/ufo.h>
#include "test-suite.h"

static void
test_constructor (void)
{
    UfoNode *node;

    node = ufo_node_new ((gpointer) 0xF00BA);
    g_assert_cmpuint (ufo_node_get_index (node), ==, 0);
    g_assert_cmpuint (ufo_node_get_total (node), ==, 1);
    g_assert (ufo_node_get_label (node) == ((gpointer) 0xF00BA));

    g_object_unref (node);
}

static void
test_copy (void)
{
    UfoNode *node;
    UfoNode *copy;
    GError *error = NULL;

    node = ufo_node_new ((gpointer) 0x123456);
    copy = ufo_node_copy (node, &error);
    g_assert_no_error (error);

    g_assert_cmpuint (ufo_node_get_index (node), ==, 0);
    g_assert_cmpuint (ufo_node_get_index (copy), ==, 1);
    g_assert_cmpuint (ufo_node_get_total (node), ==, 2);
    g_assert_cmpuint (ufo_node_get_total (copy), ==, 2);
    g_assert (ufo_node_get_label (copy) == ((gpointer) 0x123456));
    g_assert (ufo_node_equal (node, copy) != TRUE);

    g_object_unref (node);
    g_object_unref (copy);
}

void
test_add_node (void)
{
    g_test_add_func ("/no-opencl/node/constructor",
                     test_constructor);

    g_test_add_func ("/no-opencl/node/copy",
                     test_copy);
}
