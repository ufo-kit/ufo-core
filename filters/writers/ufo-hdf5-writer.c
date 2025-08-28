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

#include "common/hdf5.h"
#include "writers/ufo-writer.h"
#include "writers/ufo-hdf5-writer.h"


struct _UfoHdf5WriterPrivate {
    gchar *dataset;
    hid_t file_id;
    hid_t dataset_id;
    guint current;
};

static void ufo_writer_interface_init (UfoWriterIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoHdf5Writer, ufo_hdf5_writer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_WRITER,
                                                ufo_writer_interface_init))

#define UFO_HDF5_WRITER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_HDF5_WRITER, UfoHdf5WriterPrivate))

UfoHdf5Writer *
ufo_hdf5_writer_new (void)
{
    return g_object_new (UFO_TYPE_HDF5_WRITER, NULL);
}

static gboolean
ufo_hdf5_writer_can_open (UfoWriter *writer,
                          const gchar *filename)
{
    return ufo_hdf5_can_open (filename);
}

static void
ufo_hdf5_writer_open (UfoWriter *writer,
                      const gchar *filename)
{
    UfoHdf5WriterPrivate *priv;
    gchar *h5_filename;
    gchar **components;

    priv = UFO_HDF5_WRITER_GET_PRIVATE (writer);
    components = g_strsplit (filename, ":", 2);

    if (components[1] == NULL) {
        g_warning ("hdf5: must specify dataset name after color");
        return;
    }

    g_free (priv->dataset);
    h5_filename = components[0];
    priv->dataset = g_strdup (components[1]);

    if (g_file_test (h5_filename, G_FILE_TEST_EXISTS))
        priv->file_id = H5Fopen (h5_filename, H5F_ACC_RDWR, H5P_DEFAULT);
    else
        priv->file_id = H5Fcreate (h5_filename, H5F_ACC_EXCL, H5P_DEFAULT, H5P_DEFAULT);

    g_strfreev (components);
    priv->current = 0;
}

static void
ufo_hdf5_writer_close (UfoWriter *writer)
{
    UfoHdf5WriterPrivate *priv;

    priv = UFO_HDF5_WRITER_GET_PRIVATE (writer);
    H5Dclose (priv->dataset_id);
    H5Fclose (priv->file_id);
}

static hid_t
make_groups (hid_t file_id, const gchar *dataset)
{
    gchar *group_path;
    gchar **elements;
    hid_t group_id;
    hid_t new_group_id;

    group_id = file_id;
    group_path = g_path_get_dirname (dataset);
    elements = g_strsplit (group_path, "/", 0);

    for (guint i = 0; elements[i] != NULL; i++) {
        if (!g_strcmp0 (elements[i], ""))
            continue;

        if (H5Lexists (group_id, elements[i], H5P_DEFAULT)) {
            new_group_id = H5Gopen (group_id, elements[i], H5P_DEFAULT);
        }
        else {
            new_group_id = H5Gcreate (group_id, elements[i], H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        }

        if (group_id != file_id)
            H5Gclose (group_id);

        group_id = new_group_id;
    }

    g_free (group_path);
    g_strfreev (elements);
    return group_id;
}

static gboolean
dataset_exists (hid_t file_id, const gchar *dataset)
{
    gchar **elements;
    gboolean found = TRUE;
    GString *path_to_check;

    elements = g_strsplit (dataset, "/", 0);
    path_to_check = g_string_new (NULL);

    for (guint i = 0; elements[i] != NULL; i++) {
        /* We have to check each and every path element to see if a dataset
         * exists ... */
        if (!g_strcmp0 (elements[i], ""))
            continue;

        if (path_to_check->len > 0)
            g_string_append_c (path_to_check, '/');

        g_string_append (path_to_check, elements[i]);

        if (!H5Lexists (file_id, path_to_check->str, H5P_DEFAULT)) {
            found = FALSE;
            break;
        }
    }

    g_string_free (path_to_check, TRUE);
    g_strfreev (elements);
    return found;
}

static hid_t
buffer_depth_to_hdf5_type (UfoBufferDepth depth)
{
    switch (depth) {
        case UFO_BUFFER_DEPTH_8U:
            return H5T_NATIVE_UINT8;
        case UFO_BUFFER_DEPTH_16U:
            return H5T_NATIVE_UINT16;
        case UFO_BUFFER_DEPTH_16S:
            return H5T_NATIVE_INT16;
        default:
            return H5T_NATIVE_FLOAT;
    }
}

static void
ufo_hdf5_writer_write (UfoWriter *writer,
                       UfoWriterImage *image)
{
    UfoHdf5WriterPrivate *priv;
    hid_t dst_dataspace_id;
    hid_t src_dataspace_id;
    hid_t mem_type;

    priv = UFO_HDF5_WRITER_GET_PRIVATE (writer);

    hsize_t offset[3] = { priv->current, 0, 0 };
    hsize_t count[3] = { 1, image->requisition->dims[1], image->requisition->dims[0] };
    hsize_t dims[3] = { priv->current + 1, image->requisition->dims[1], image->requisition->dims[0] };
    hsize_t src_dims[2] = { image->requisition->dims[0], image->requisition->dims[1] };
    mem_type = buffer_depth_to_hdf5_type (image->depth);

    if (priv->current == 0) {
        if (dataset_exists (priv->file_id, priv->dataset)) {
            priv->dataset_id = H5Dopen (priv->file_id, priv->dataset, H5P_DEFAULT);
        }
        else {
            hid_t group_id;
            hid_t dcpl;
            hsize_t max_dims[3] = { H5S_UNLIMITED, image->requisition->dims[1], image->requisition->dims[0] };

            group_id = make_groups (priv->file_id, priv->dataset);

            dst_dataspace_id = H5Screate_simple (3, dims, max_dims);
            dcpl = H5Pcreate (H5P_DATASET_CREATE);
            H5Pset_chunk (dcpl, 3, dims);
            priv->dataset_id = H5Dcreate (group_id, priv->dataset, mem_type, dst_dataspace_id,
                                          H5P_DEFAULT, dcpl, H5P_DEFAULT);

            H5Pclose (dcpl);
            H5Sclose (dst_dataspace_id);
        }
    }
    else {
        H5Dset_extent (priv->dataset_id, dims);
    }

    dst_dataspace_id = H5Dget_space (priv->dataset_id);
    src_dataspace_id = H5Screate_simple (2, src_dims, NULL);

    H5Sselect_hyperslab (dst_dataspace_id, H5S_SELECT_SET, offset, NULL, count, NULL);
    H5Dwrite (priv->dataset_id, mem_type, src_dataspace_id, dst_dataspace_id, H5P_DEFAULT, image->data);

    H5Sclose (src_dataspace_id);
    H5Sclose (dst_dataspace_id);
    priv->current++;
}

static void
ufo_hdf5_writer_finalize (GObject *object)
{
    UfoHdf5WriterPrivate *priv;

    priv = UFO_HDF5_WRITER_GET_PRIVATE (object);
    g_free (priv->dataset);

    G_OBJECT_CLASS (ufo_hdf5_writer_parent_class)->finalize (object);
}

static void
ufo_writer_interface_init (UfoWriterIface *iface)
{
    iface->can_open = ufo_hdf5_writer_can_open;
    iface->open = ufo_hdf5_writer_open;
    iface->close = ufo_hdf5_writer_close;
    iface->write = ufo_hdf5_writer_write;
}

static void
ufo_hdf5_writer_class_init (UfoHdf5WriterClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = ufo_hdf5_writer_finalize;

    g_type_class_add_private (gobject_class, sizeof (UfoHdf5WriterPrivate));
}

static void
ufo_hdf5_writer_init (UfoHdf5Writer *self)
{
    UfoHdf5WriterPrivate *priv = NULL;

    self->priv = priv = UFO_HDF5_WRITER_GET_PRIVATE (self);
    priv->dataset = NULL;
}
