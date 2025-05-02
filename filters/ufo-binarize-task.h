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

#ifndef __UFO_BINARIZE_TASK_H
#define __UFO_BINARIZE_TASK_H

#include <ufo/ufo.h>

G_BEGIN_DECLS

#define UFO_TYPE_BINARIZE_TASK             (ufo_binarize_task_get_type())
#define UFO_BINARIZE_TASK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_BINARIZE_TASK, UfoBinarizeTask))
#define UFO_IS_BINARIZE_TASK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_BINARIZE_TASK))
#define UFO_BINARIZE_TASK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), UFO_TYPE_BINARIZE_TASK, UfoBinarizeTaskClass))
#define UFO_IS_BINARIZE_TASK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), UFO_TYPE_BINARIZE_TASK))
#define UFO_BINARIZE_TASK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UFO_TYPE_BINARIZE_TASK, UfoBinarizeTaskClass))

typedef struct _UfoBinarizeTask           UfoBinarizeTask;
typedef struct _UfoBinarizeTaskClass      UfoBinarizeTaskClass;
typedef struct _UfoBinarizeTaskPrivate    UfoBinarizeTaskPrivate;

struct _UfoBinarizeTask {
    UfoTaskNode parent_instance;

    UfoBinarizeTaskPrivate *priv;
};

struct _UfoBinarizeTaskClass {
    UfoTaskNodeClass parent_class;
};

UfoNode  *ufo_binarize_task_new       (void);
GType     ufo_binarize_task_get_type  (void);

G_END_DECLS

#endif

