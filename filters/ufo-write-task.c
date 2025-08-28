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

#include "config.h"
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <glib/gstdio.h>
#include <gmodule.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "ufo-write-task.h"
#include "writers/ufo-writer.h"
#include "writers/ufo-raw-writer.h"

#ifdef HAVE_TIFF
#include "writers/ufo-tiff-writer.h"
#endif

#ifdef HAVE_JPEG
#include "writers/ufo-jpeg-writer.h"
#endif

#ifdef WITH_HDF5
#include "writers/ufo-hdf5-writer.h"
#endif

struct _UfoWriteTaskPrivate {
    gchar *filename;
    guint counter;
    guint counter_start;
    guint counter_step;
    gulong bytes_per_file;
    gulong num_written_bytes;
    gboolean append;
    gsize width;
    gsize height;

    UfoBufferDepth depth;
    guint bits_per_sample;
    gfloat minimum;
    gfloat maximum;
    gboolean rescale;

    guint num_fmt_specifiers;
    gboolean opened;

    cl_context context;
    cl_kernel kernel;
    UfoBuffer *tmp;

    UfoWriter     *writer;
    UfoRawWriter  *raw_writer;

#ifdef HAVE_TIFF
    UfoTiffWriter *tiff_writer;
#endif

#ifdef HAVE_JPEG
    UfoJpegWriter *jpeg_writer;
    gint           jpeg_quality;
#endif

#ifdef WITH_HDF5
    UfoHdf5Writer *hdf5_writer;
#endif
};

static void ufo_task_interface_init (UfoTaskIface *iface);

G_DEFINE_TYPE_WITH_CODE (UfoWriteTask, ufo_write_task, UFO_TYPE_TASK_NODE,
                         G_IMPLEMENT_INTERFACE (UFO_TYPE_TASK,
                                                ufo_task_interface_init))

#define UFO_WRITE_TASK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_WRITE_TASK, UfoWriteTaskPrivate))

enum {
    PROP_0,
    PROP_FILENAME,
    PROP_COUNTER_START,
    PROP_COUNTER_STEP,
    PROP_BYTES_PER_FILE,
    PROP_APPEND,
    PROP_BITS,
    PROP_MINIMUM,
    PROP_MAXIMUM,
    PROP_RESCALE,
#ifdef HAVE_JPEG
    PROP_JPEG_QUALITY,
#endif
#ifdef HAVE_TIFF
    PROP_TIFF_BIGTIFF,
#endif
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

UfoNode *
ufo_write_task_new (void)
{
    return UFO_NODE (g_object_new (UFO_TYPE_WRITE_TASK, NULL));
}

static gchar *
get_current_filename (UfoWriteTaskPrivate *priv)
{
    if (!priv->filename) {
        return NULL;
    }

    if (!priv->num_fmt_specifiers)
        return g_strdup (priv->filename);

    return g_strdup_printf (priv->filename, priv->counter);
}

static guint
count_format_specifiers (const gchar *filename)
{
    guint count = 0;

    do {
        if (*filename == '%')
            count++;
    } while (*(filename++));

    return count;
}

static gboolean
can_be_written (const gchar *filename, GError **error)
{
    if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
        if (g_access (filename, W_OK) < 0) {
            g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno),
                         "Cannot access `%s': %s.", filename, strerror (errno));
            return FALSE;
        }
    }

    return TRUE;
}

static void
ufo_write_task_setup (UfoTask *task,
                      UfoResources *resources,
                      GError **error)
{
    UfoWriteTaskPrivate *priv;
    gchar *dirname;

    priv = UFO_WRITE_TASK_GET_PRIVATE (task);

    /* If no filename has been specified we write to stdout */
    if (priv->filename == NULL) {
        priv->writer = UFO_WRITER (priv->raw_writer);
        return;
    }

    priv->num_fmt_specifiers = count_format_specifiers (priv->filename);
    dirname = g_path_get_dirname (priv->filename);

    if (priv->num_fmt_specifiers > 1) {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "`%s' has too many format specifiers", dirname);
        return;
    }

    /* Check that we can actually overwrite existing files */
    if (!priv->num_fmt_specifiers && !can_be_written (priv->filename, error))
        return;

    if (!priv->append) {
        priv->counter = 0;
    }
    priv->num_written_bytes = 0;

    if (ufo_writer_can_open (UFO_WRITER (priv->raw_writer), priv->filename)) {
        priv->writer = UFO_WRITER (priv->raw_writer);
    }
#ifdef HAVE_TIFF
    else if (ufo_writer_can_open (UFO_WRITER (priv->tiff_writer), priv->filename)) {
        priv->writer = UFO_WRITER (priv->tiff_writer);
    }
#endif
#ifdef WITH_HDF5
    else if (ufo_writer_can_open (UFO_WRITER (priv->hdf5_writer), priv->filename)) {
        gchar **components;

        priv->writer = UFO_WRITER (priv->hdf5_writer);

        /*
         * dirname will be wrong because we use path separators for the dataset.
         * So, lets' find it out manually.
         */
        g_free (dirname);
        components = g_strsplit (priv->filename, ":", 2);
        dirname = g_path_get_dirname (components[0]);
        g_strfreev (components);
    }
#endif
#ifdef HAVE_JPEG
    else if (ufo_writer_can_open (UFO_WRITER (priv->jpeg_writer), priv->filename)) {
        priv->writer = UFO_WRITER (priv->jpeg_writer);
    }
#endif
    else {
        g_set_error (error, UFO_TASK_ERROR, UFO_TASK_ERROR_SETUP,
                     "`%s' does not have a valid file extension or requires format specifiers", priv->filename);
        return;
    }

    if (!g_file_test (dirname, G_FILE_TEST_EXISTS)) {
        g_debug ("write: `%s' does not exist. Attempt to create it.", dirname);

        if (g_mkdir_with_parents (dirname, 0755)) {
            g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                         "Could not create `%s'.", dirname);
            return;
        }
    }

    priv->counter = priv->counter_start;

    if (priv->append && priv->num_fmt_specifiers) {
        gboolean exists = TRUE;

        while (exists) {
            gchar *filename = get_current_filename (priv);
            exists = g_file_test (filename, G_FILE_TEST_EXISTS);
            g_free (filename);

            if (exists)
                priv->counter++;
        }
    }

    g_free (dirname);

    priv->context = ufo_resources_get_context (resources);
    UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainContext (priv->context), error);

    priv->kernel = ufo_resources_get_kernel (resources, "split.cl", "unsplit", NULL, error);

    if (priv->kernel != NULL)
        UFO_RESOURCES_CHECK_SET_AND_RETURN (clRetainKernel (priv->kernel), error);
}

static void
ufo_write_task_get_requisition (UfoTask *task,
                                UfoBuffer **inputs,
                                UfoRequisition *requisition,
                                GError **error)
{
    requisition->n_dims = 0;
}

static guint
ufo_write_task_get_num_inputs (UfoTask *task)
{
    return 1;
}

static guint
ufo_write_task_get_num_dimensions (UfoTask *task,
                               guint input)
{
    g_return_val_if_fail (input == 0, 0);
    return 2;
}

static UfoTaskMode
ufo_write_task_get_mode (UfoTask *task)
{
    return UFO_TASK_MODE_SINK | UFO_TASK_MODE_GPU;
}

static gboolean
ufo_write_task_process (UfoTask *task,
                        UfoBuffer **inputs,
                        UfoBuffer *output,
                        UfoRequisition *requisition)
{
    UfoWriteTaskPrivate *priv;
    UfoWriterImage image;
    UfoRequisition in_req;
    guint8 *data;
    guint num_frames;
    gsize offset, out_size;

    priv = UFO_WRITE_TASK_GET_PRIVATE (UFO_WRITE_TASK (task));
    ufo_buffer_get_requisition (inputs[0], &in_req);

    /* 
     * If we have a cube with a depth of three planes we try to write color
     * images further down the line otherwise split it up and write the planes
     * as single files.
     */
    if (in_req.n_dims == 3 && in_req.dims[2] == 3) {
        UfoGpuNode *node;
        UfoProfiler *profiler;
        cl_command_queue cmd_queue;
        cl_mem in_mem;
        cl_mem out_mem;

        if (!priv->tmp)
            priv->tmp = ufo_buffer_new (&in_req, priv->context);

        node = UFO_GPU_NODE (ufo_task_node_get_proc_node (UFO_TASK_NODE (task)));
        cmd_queue = ufo_gpu_node_get_cmd_queue (node);
        in_mem = ufo_buffer_get_device_array (inputs[0], cmd_queue);
        out_mem = ufo_buffer_get_device_array (priv->tmp, cmd_queue);

        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 0, sizeof (cl_mem), &in_mem));
        UFO_RESOURCES_CHECK_CLERR (clSetKernelArg (priv->kernel, 1, sizeof (cl_mem), &out_mem));
        profiler = ufo_task_node_get_profiler (UFO_TASK_NODE (task));
        ufo_profiler_call (profiler, cmd_queue, priv->kernel, 3, in_req.dims, NULL);

        num_frames = 1;
        data = (guint8 *) ufo_buffer_get_host_array (priv->tmp, NULL);
    }
    else {
        num_frames = in_req.n_dims == 3 ? in_req.dims[2] : 1;
        data = (guint8 *) ufo_buffer_get_host_array (inputs[0], NULL);
    }

    offset = ufo_buffer_get_size (inputs[0]) / num_frames;
    out_size = in_req.dims[0] * in_req.dims[1] * priv->bits_per_sample / 8;

    image.requisition = &in_req;
    image.depth = priv->depth;
    image.min = priv->minimum;
    image.max = priv->maximum;
    image.rescale = priv->rescale;

    for (guint i = 0; i < num_frames; i++) {
retry:
        if (!priv->opened) {
            GError *error = NULL;
            gchar *filename = get_current_filename (priv);

            if (filename && !can_be_written (filename, &error)) {
                g_warning ("%s", error->message);
                g_free (filename);
                g_error_free (error);
                priv->counter += priv->counter_step;
                goto retry;
            }

            ufo_writer_open (priv->writer, filename);
            if (filename) {
                g_free (filename);
            }
            priv->opened = TRUE;
        }

        image.data = data + i * offset;
        ufo_writer_write (priv->writer, &image);
        priv->num_written_bytes += out_size;

        if (priv->num_fmt_specifiers && priv->num_written_bytes + out_size > priv->bytes_per_file) {
            ufo_writer_close (priv->writer);
            priv->opened = FALSE;
            priv->num_written_bytes = 0;
            priv->counter += priv->counter_step;
        }
    }

    return TRUE;
}

static void
ufo_write_task_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    UfoWriteTaskPrivate *priv = UFO_WRITE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_FILENAME:
            g_free (priv->filename);
            priv->filename = g_value_dup_string (value);
            break;
        case PROP_COUNTER_START:
            priv->counter_start = g_value_get_uint (value);
            break;
        case PROP_COUNTER_STEP:
            priv->counter_step = g_value_get_uint (value);
            break;
        case PROP_BYTES_PER_FILE:
            priv->bytes_per_file = g_value_get_ulong (value);
            break;
        case PROP_APPEND:
            priv->append = g_value_get_boolean (value);
            break;
        case PROP_BITS:
            {
                guint val = g_value_get_uint (value);

                if (val != 8 && val != 16 && val != 32) {
                    g_warning ("Write::bits can only 8, 16 or 32");
                    return;
                }

                if (val == 8) {
                    priv->depth = UFO_BUFFER_DEPTH_8U;
                    priv->bits_per_sample = 8;
                }

                if (val == 16) {
                    priv->depth = UFO_BUFFER_DEPTH_16U;
                    priv->bits_per_sample = 16;
                }

                if (val == 32) {
                    priv->depth = UFO_BUFFER_DEPTH_32F;
                    priv->bits_per_sample = 16;
                }
            }
            break;
        case PROP_MAXIMUM:
            priv->maximum = g_value_get_float (value);
            break;
        case PROP_MINIMUM:
            priv->minimum = g_value_get_float (value);
            break;
        case PROP_RESCALE:
            priv->rescale = g_value_get_boolean (value);
            break;
#ifdef HAVE_JPEG
        case PROP_JPEG_QUALITY:
            priv->jpeg_quality = g_value_get_uint (value);
            ufo_jpeg_writer_set_quality (priv->jpeg_writer, priv->jpeg_quality);
            break;
#endif
#ifdef HAVE_TIFF
        case PROP_TIFF_BIGTIFF:
            g_object_set_property (G_OBJECT (priv->tiff_writer), "bigtiff", value);
            break;
#endif
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_write_task_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    UfoWriteTaskPrivate *priv = UFO_WRITE_TASK_GET_PRIVATE (object);

    switch (property_id) {
        case PROP_FILENAME:
            g_value_set_string (value, priv->filename ? priv->filename : "");
            break;
        case PROP_COUNTER_START:
            g_value_set_uint (value, priv->counter_start);
            break;
        case PROP_COUNTER_STEP:
            g_value_set_uint (value, priv->counter_step);
            break;
        case PROP_BYTES_PER_FILE:
            g_value_set_ulong (value, priv->bytes_per_file);
            break;
        case PROP_APPEND:
            g_value_set_boolean (value, priv->append);
            break;
        case PROP_BITS:
            if (priv->depth == UFO_BUFFER_DEPTH_8U)
                g_value_set_uint (value, 8);

            if (priv->depth == UFO_BUFFER_DEPTH_16U)
                g_value_set_uint (value, 16);

            if (priv->depth == UFO_BUFFER_DEPTH_32F)
                g_value_set_uint (value, 32);
            break;
        case PROP_MAXIMUM:
            g_value_set_float (value, priv->maximum);
            break;
        case PROP_MINIMUM:
            g_value_set_float (value, priv->minimum);
            break;
        case PROP_RESCALE:
            g_value_set_boolean (value, priv->rescale);
            break;
#ifdef HAVE_JPEG
        case PROP_JPEG_QUALITY:
            g_value_set_uint (value, priv->jpeg_quality);
            break;
#endif
#ifdef HAVE_TIFF
        case PROP_TIFF_BIGTIFF:
            g_object_get_property (G_OBJECT (priv->tiff_writer), "bigtiff", value);
            break;
#endif
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
ufo_write_task_dispose (GObject *object)
{
    UfoWriteTaskPrivate *priv;

    priv = UFO_WRITE_TASK_GET_PRIVATE (object);

    g_object_unref (priv->raw_writer);

#ifdef HAVE_TIFF
    if (priv->tiff_writer)
        g_object_unref (priv->tiff_writer);
#endif

#ifdef HAVE_JPEG
    if (priv->jpeg_writer)
        g_object_unref (priv->jpeg_writer);
#endif

#ifdef WITH_HDF5
    if (priv->hdf5_writer != NULL)
        g_object_unref (priv->hdf5_writer);
#endif

    G_OBJECT_CLASS (ufo_write_task_parent_class)->dispose (object);
}

static void
ufo_write_task_finalize (GObject *object)
{
    UfoWriteTaskPrivate *priv;

    priv = UFO_WRITE_TASK_GET_PRIVATE (object);

    if (priv->filename) {
        g_free (priv->filename);
        priv->filename= NULL;
    }

    if (priv->kernel) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseKernel (priv->kernel));
        priv->kernel = NULL;
    }

    if (priv->tmp) {
        g_object_unref (priv->tmp);
        priv->tmp = NULL;
    }

    if (priv->context) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseContext (priv->context));
        priv->context = NULL;
    }

    G_OBJECT_CLASS (ufo_write_task_parent_class)->finalize (object);
}

static void
ufo_task_interface_init (UfoTaskIface *iface)
{
    iface->setup = ufo_write_task_setup;
    iface->get_num_inputs = ufo_write_task_get_num_inputs;
    iface->get_num_dimensions = ufo_write_task_get_num_dimensions;
    iface->get_mode = ufo_write_task_get_mode;
    iface->get_requisition = ufo_write_task_get_requisition;
    iface->process = ufo_write_task_process;
}

static void
ufo_write_task_class_init (UfoWriteTaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ufo_write_task_set_property;
    gobject_class->get_property = ufo_write_task_get_property;
    gobject_class->dispose = ufo_write_task_dispose;
    gobject_class->finalize = ufo_write_task_finalize;

    properties[PROP_FILENAME] =
        g_param_spec_string ("filename",
            "Filename filename string",
            "filename string of the path and filename. "
                "If multiple files are written it must contain a '%i' specifier denoting the current count",
            "",
            G_PARAM_READWRITE);

    properties[PROP_COUNTER_START] =
        g_param_spec_uint ("counter-start",
            "Start of filename counter",
            "Start of filename counter",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE);

    properties[PROP_COUNTER_STEP] =
        g_param_spec_uint ("counter-step",
            "Step of filename counter",
            "Step of filename counter",
            1, G_MAXUINT, 1,
            G_PARAM_READWRITE);

    properties[PROP_BYTES_PER_FILE] =
        g_param_spec_ulong ("bytes-per-file",
            "Bytes per file for multi-page files",
            "Bytes per file for multi-page files",
            0, G_MAXULONG, 137438953472,
            G_PARAM_READWRITE);

    properties[PROP_APPEND] =
        g_param_spec_boolean ("append",
            "If true the data is appended, otherwise overwritten",
            "If true the data is appended, otherwise overwritten",
            FALSE,
            G_PARAM_READWRITE);

    properties[PROP_BITS] =
        g_param_spec_uint ("bits",
            "Number of bits per sample",
            "Number of bits per sample. Possible values in [8, 16, 32].",
            8, 32, 32, G_PARAM_READWRITE);

    properties[PROP_MINIMUM] =
        g_param_spec_float ("minimum",
            "Lowest value to be used for spreading",
            "Lowest value to be used for spreading",
            -G_MAXFLOAT, G_MAXFLOAT, G_MAXFLOAT,
            G_PARAM_READWRITE);

    properties[PROP_MAXIMUM] =
        g_param_spec_float ("maximum",
            "Highest value to be used for spreading",
            "Highest value to be used for spreading",
            -G_MAXFLOAT, G_MAXFLOAT, -G_MAXFLOAT,
            G_PARAM_READWRITE);

    properties[PROP_RESCALE] =
        g_param_spec_boolean ("rescale",
            "If true rescale values automatically or according to set min and max",
            "If true rescale values automatically or according to set min and max",
            TRUE,
            G_PARAM_READWRITE);

#ifdef HAVE_JPEG
    properties[PROP_JPEG_QUALITY] =
        g_param_spec_uint ("jpeg-quality",
            "JPEG quality",
            "JPEG quality between 0 and 100",
            0, 100, 95, G_PARAM_READWRITE);
#endif

#ifdef HAVE_TIFF
    properties[PROP_TIFF_BIGTIFF] =
        g_param_spec_boolean("tiff-bigtiff",
            "Write BigTiff format",
            "Write BigTiff format",
            TRUE,
            G_PARAM_READWRITE);
#endif

    for (guint i = PROP_0 + 1; i < N_PROPERTIES; i++)
        g_object_class_install_property (gobject_class, i, properties[i]);

    g_type_class_add_private (gobject_class, sizeof(UfoWriteTaskPrivate));
}

static void
ufo_write_task_init(UfoWriteTask *self)
{
    self->priv = UFO_WRITE_TASK_GET_PRIVATE(self);
    self->priv->counter = 0;
    self->priv->counter_start = 0;
    self->priv->counter_step = 1;
    self->priv->bytes_per_file = ((gulong) 1) << 37;
    self->priv->num_written_bytes = 0;
    self->priv->num_fmt_specifiers = 0;
    self->priv->append = FALSE;
    self->priv->bits_per_sample = 32;
    self->priv->depth = UFO_BUFFER_DEPTH_32F;
    self->priv->minimum = G_MAXFLOAT;
    self->priv->maximum = -G_MAXFLOAT;
    self->priv->rescale = TRUE;
    self->priv->writer = NULL;
    self->priv->opened = FALSE;
    self->priv->filename = NULL;
    self->priv->raw_writer = ufo_raw_writer_new ();
    self->priv->context = NULL;
    self->priv->kernel = NULL;
    self->priv->tmp = NULL;

#ifdef HAVE_TIFF
    self->priv->tiff_writer = ufo_tiff_writer_new ();
#endif

#ifdef HAVE_JPEG
    self->priv->jpeg_writer = ufo_jpeg_writer_new ();
    self->priv->jpeg_quality = 95;
#endif

#ifdef WITH_HDF5
    self->priv->hdf5_writer = ufo_hdf5_writer_new ();
#endif
}
