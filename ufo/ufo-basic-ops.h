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

#ifndef __UFO_BASIC_OPS
#define __UFO_BASIC_OPS

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#include <glib-object.h>
#include <ufo/ufo-buffer.h>
#include <ufo/ufo-resources.h>

G_BEGIN_DECLS

gpointer ufo_op_set         (UfoBuffer      *arg,
                             gfloat           value,
                             UfoResources   *resources,
                             gpointer        command_queue);
gpointer ufo_op_inv         (UfoBuffer      *arg,
                             UfoResources   *resources,
                             gpointer        command_queue);
gpointer ufo_op_mul         (UfoBuffer      *arg1,
                             UfoBuffer      *arg2,
                             UfoBuffer      *out,
                             UfoResources   *resources,
                             gpointer        command_queue);
gpointer ufo_op_add         (UfoBuffer      *arg1,
                             UfoBuffer      *arg2,
                             UfoBuffer      *out,
                             UfoResources   *resources,
                             gpointer        command_queue);
gpointer ufo_op_add2        (UfoBuffer      *arg1,
                             UfoBuffer      *arg2,
                             gfloat          modifier,
                             UfoBuffer      *out,
                             UfoResources   *resources,
                             gpointer        command_queue);
gpointer ufo_op_deduction   (UfoBuffer      *arg1,
                             UfoBuffer      *arg2,
                             UfoBuffer      *out,
                             UfoResources   *resources,
                             gpointer        command_queue);
gpointer ufo_op_deduction2  (UfoBuffer      *arg1,
                             UfoBuffer      *arg2,
                             gfloat          modifier,
                             UfoBuffer      *out,
                             UfoResources   *resources,
                             gpointer        command_queue);
gpointer ufo_op_mul_rows    (UfoBuffer      *arg1,
                             UfoBuffer      *arg2,
                             UfoBuffer      *out,
                             guint           offset,
                             guint           n,
                             UfoResources   *resources,
                             gpointer        command_queue);
gpointer ufo_op_gradient_magnitudes
                            (UfoBuffer      *arg,
                             UfoBuffer      *out,
                             UfoResources   *resources,
                             gpointer        command_queue);
gpointer ufo_op_gradient_directions
                            (UfoBuffer      *arg,
                             UfoBuffer      *magnitudes,
                             UfoBuffer      *out,
                             UfoResources   *resources,
                             gpointer        command_queue);
gfloat ufo_op_l1_norm       (UfoBuffer      *arg,
                             UfoResources   *resources,
                             gpointer        command_queue);
gfloat ufo_op_l2_norm       (UfoBuffer      *arg,
                             UfoResources   *resources,
                             gpointer        command_queue);
gfloat ufo_op_euclidean_distance
                            (UfoBuffer      *arg1,
                             UfoBuffer      *arg2,
                             UfoResources   *resources,
                             gpointer        command_queue);
gpointer ufo_op_POSC        (UfoBuffer      *arg,
                             UfoBuffer      *out,
                             UfoResources   *resources,
                             gpointer        command_queue);
gpointer ufo_op_gradient_descent
                            (UfoBuffer      *arg,
                             UfoBuffer      *out,
                             UfoResources   *resources,
                             gpointer        command_queue);

G_END_DECLS

#endif
