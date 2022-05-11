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

#ifndef UFO_READER_H
#define UFO_READER_H

#include <ufo/ufo.h>

G_BEGIN_DECLS

#define UFO_TYPE_READER             (ufo_reader_get_type())
#define UFO_READER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), UFO_TYPE_READER, UfoReader))
#define UFO_IS_READER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), UFO_TYPE_READER))
#define UFO_READER_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE((inst), UFO_TYPE_READER, UfoReaderIface))


typedef struct _UfoReader         UfoReader;
typedef struct _UfoReaderIface    UfoReaderIface;


struct _UfoReaderIface {
    /*< private >*/
    GTypeInterface parent_iface;

    gboolean    (*can_open)             (UfoReader      *reader,
                                         const gchar    *filename);
    gboolean    (*open)                 (UfoReader      *reader,
                                         const gchar    *filename,
                                         guint           start,
                                         GError        **error);
    void        (*close)                (UfoReader      *reader);
    gboolean    (*data_available)       (UfoReader      *reader);
    gboolean    (*get_meta)             (UfoReader      *reader,
                                         UfoRequisition *requisition,
                                         gsize          *num_images,
                                         guint          *bitdepth,
                                         GError        **error);
    gsize       (*read)                 (UfoReader      *reader,
                                         UfoBuffer      *buffer,
                                         UfoRequisition *requisition,
                                         guint           roi_y,
                                         guint           roi_height,
                                         guint           roi_step,
                                         guint           image_step);
};

gboolean    ufo_reader_can_open         (UfoReader      *reader,
                                         const gchar    *filename);
gboolean    ufo_reader_open             (UfoReader      *reader,
                                         const gchar    *filename,
                                         guint           start,
                                         GError        **error);
void        ufo_reader_close            (UfoReader      *reader);
gboolean    ufo_reader_data_available   (UfoReader      *reader);
gboolean    ufo_reader_get_meta         (UfoReader      *reader,
                                         UfoRequisition *requisition,
                                         gsize          *num_images,
                                         UfoBufferDepth *bitdepth,
                                         GError        **error);
gsize       ufo_reader_read             (UfoReader      *reader,
                                         UfoBuffer      *buffer,
                                         UfoRequisition *requisition,
                                         guint           roi_y,
                                         guint           roi_height,
                                         guint           roi_step,
                                         guint           image_step);

GType  ufo_reader_get_type        (void);

G_END_DECLS

#endif

