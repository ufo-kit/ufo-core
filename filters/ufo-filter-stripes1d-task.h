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

#ifndef __UFO_FILTER_STRIPES1D_TASK_H
#define __UFO_FILTER_STRIPES1D_TASK_H

#include <ufo/ufo.h>

G_BEGIN_DECLS

#define UFO_TYPE_FILTER_STRIPES1D_TASK             (ufo_filter_stripes1d_task_get_type())
#define UFO_FILTER_STRIPES1D_TASK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_FILTER_STRIPES1D_TASK, UfoFilterStripes1dTask))
#define UFO_IS_FILTER_STRIPES1D_TASK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_FILTER_STRIPES1D_TASK))
#define UFO_FILTER_STRIPES1D_TASK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_FILTER_STRIPES1D_TASK, UfoFilterStripes1dTaskClass))
#define UFO_IS_FILTER_STRIPES1D_TASK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_FILTER_STRIPES1D_TASK))
#define UFO_FILTER_STRIPES1D_TASK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_FILTER_STRIPES1D_TASK, UfoFilterStripes1dTaskClass))

typedef struct _UfoFilterStripes1dTask           UfoFilterStripes1dTask;
typedef struct _UfoFilterStripes1dTaskClass      UfoFilterStripes1dTaskClass;
typedef struct _UfoFilterStripes1dTaskPrivate    UfoFilterStripes1dTaskPrivate;

struct _UfoFilterStripes1dTask {
    UfoTaskNode parent_instance;

    UfoFilterStripes1dTaskPrivate *priv;
};

struct _UfoFilterStripes1dTaskClass {
    UfoTaskNodeClass parent_class;
};

UfoNode  *ufo_filter_stripes1d_task_new       (void);
GType     ufo_filter_stripes1d_task_get_type  (void);

G_END_DECLS

#endif

