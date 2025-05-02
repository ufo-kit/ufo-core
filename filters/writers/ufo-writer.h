/*
 * Copyright (C) 2011-2015 Karlsruhe Institute of Technology
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

#ifndef UFO_WRITER_H
#define UFO_WRITER_H

#include <ufo/ufo.h>

G_BEGIN_DECLS

#define UFO_TYPE_WRITER             (ufo_writer_get_type())
#define UFO_WRITER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_WRITER, UfoWriter))
#define UFO_IS_WRITER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_WRITER))
#define UFO_WRITER_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE((inst), UFO_TYPE_WRITER, UfoWriterIface))


typedef struct _UfoWriter         UfoWriter;
typedef struct _UfoWriterIface    UfoWriterIface;

typedef struct {
    gpointer data;
    UfoRequisition *requisition;
    UfoBufferDepth depth;
    gfloat min;
    gfloat max;
    gboolean rescale;
} UfoWriterImage;

struct _UfoWriterIface {
    /*< private >*/
    GTypeInterface parent_iface;

    gboolean (*can_open) (UfoWriter     *writer,
                          const gchar   *filename);
    void     (*open)     (UfoWriter      *writer,
                          const gchar    *filename);
    void     (*close)    (UfoWriter      *writer);
    void     (*write)    (UfoWriter      *writer,
                          UfoWriterImage *image);
};

gboolean ufo_writer_can_open (UfoWriter      *writer,
                              const gchar    *filename);
void     ufo_writer_open     (UfoWriter      *writer,
                              const gchar    *filename);
void     ufo_writer_close    (UfoWriter      *writer);
void     ufo_writer_write    (UfoWriter      *writer,
                              UfoWriterImage *image);
void     ufo_writer_convert_inplace
                             (UfoWriterImage *image);

GType  ufo_writer_get_type        (void);

G_END_DECLS

#endif

