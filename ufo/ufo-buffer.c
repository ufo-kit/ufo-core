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

#include <string.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <ufo/ufo-buffer.h>
#include <ufo/ufo-resources.h>
#include "compat.h"

/**
 * SECTION:ufo-buffer
 * @Short_description: Manages and represents n-dimensional data
 * @Title: UfoBuffer
 */

/**
 * UFO_BUFFER_MAX_NDIMS:
 *
 * Maximum number of allowed dimensions. This is a pre-processor macro instead
 * of const variable because of <ulink
 * url="http://c-faq.com/ansi/constasconst.html">C constraints</ulink>.
 */

/**
 * UfoRequisition:
 * @n_dims: Number of dimensions
 * @dims: Size of dimension
 *
 * Used to specify buffer size requirements.
 */

/**
 * UfoRegion:
 * @origin: n-dimensional origin of the region
 * @size: n-dimensional size of the region
 *
 * Defines a region with at most #UFO_BUFFER_MAX_NDIMS dimensions for use with
 * ufo_buffer_get_device_array_view().
 */

/**
 * UfoBufferDepth:
 * @UFO_BUFFER_DEPTH_8U: 8 bit unsigned
 * @UFO_BUFFER_DEPTH_16U: 16 bit unsigned
 * @UFO_BUFFER_DEPTH_16S: 16 bit signed
 * @UFO_BUFFER_DEPTH_32S: 32 bit signed
 * @UFO_BUFFER_DEPTH_32U: 32 bit unsigned
 * @UFO_BUFFER_DEPTH_32F: 32 bit float
 *
 * Source depth of data as used in ufo_buffer_convert().
 */

/**
 * UfoBufferLocation:
 * @UFO_BUFFER_LOCATION_HOST: Data is located in main memory
 * @UFO_BUFFER_LOCATION_DEVICE: Data is located in regular device memory
 * @UFO_BUFFER_LOCATION_DEVICE_IMAGE: Data is located in image device memory
 * @UFO_BUFFER_LOCATION_INVALID: There is currently no data associated with the
 *  buffer
 *
 * Location of the backed data memory.
 */

G_DEFINE_TYPE(UfoBuffer, ufo_buffer, G_TYPE_OBJECT)

#define UFO_BUFFER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UFO_TYPE_BUFFER, UfoBufferPrivate))

enum {
    PROP_0,
    PROP_ID,
    PROP_STRUCTURE,
    N_PROPERTIES
};

struct _UfoBufferPrivate {
    UfoRequisition      requisition;
    gfloat             *host_array;
    gboolean            free;
    cl_mem              device_array;
    cl_mem              device_image;
    cl_context          context;
    cl_command_queue    last_queue;
    gsize               size;           /* size of buffer in bytes */
    UfoBufferLocation      location;
    UfoBufferLocation      last_location;
    GHashTable         *metadata;
    GList              *sub_device_arrays;
};

static void
copy_requisition (UfoRequisition *src,
                  UfoRequisition *dst)
{
    const guint n_dims = src->n_dims;

    dst->n_dims = n_dims;

    for (guint i = 0; i < n_dims; i++)
        dst->dims[i] = src->dims[i];
}

static gsize
compute_required_size (UfoRequisition *requisition)
{
    gsize size = sizeof (gfloat);

    for (guint i = 0; i < requisition->n_dims; i++)
        size *= requisition->dims[i];

    return size;
}

static void
alloc_host_mem (UfoBufferPrivate *priv)
{
    if (priv->host_array != NULL && priv->free)
        g_free (priv->host_array);

    priv->host_array = g_malloc0 (priv->size);
}

static void
alloc_device_array (UfoBufferPrivate *priv)
{
    cl_int err;
    cl_mem mem;

    if (priv->device_array != NULL)
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->device_array));

    mem = clCreateBuffer (priv->context,
                          CL_MEM_READ_WRITE,
                          priv->size,
                          NULL, &err);

    UFO_RESOURCES_CHECK_CLERR (err);
    priv->device_array = mem;
}

#if 0
static void
alloc_device_image (UfoBufferPrivate *priv)
{
    cl_image_desc desc;
    cl_image_format format;
    cl_int errcode;
    cl_mem mem;

    g_assert ((priv->requisition.n_dims == 2) ||
              (priv->requisition.n_dims == 3));

    if (priv->device_image != NULL)
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->device_image));

    format.image_channel_order = CL_INTENSITY;
    format.image_channel_data_type = CL_FLOAT;

    if (priv->requisition.n_dims == 2) {
        desc.image_type = CL_MEM_OBJECT_IMAGE2D;
        desc.image_depth = 1;
    }
    else {
        desc.image_type = CL_MEM_OBJECT_IMAGE3D;
        desc.image_depth = priv->requisition.dims[2];
    }

    desc.image_width = priv->requisition.dims[0];
    desc.image_height = priv->requisition.dims[1];
    desc.image_array_size = 0;
    desc.image_row_pitch = 0;
    desc.image_slice_pitch = 0;
    desc.num_mip_levels = 0;
    desc.num_samples = 0;
    desc.buffer = NULL;

    mem = clCreateImage (priv->context,
                         CL_MEM_READ_WRITE,
                         &format, &desc,
                         NULL, &errcode);

    UFO_RESOURCES_CHECK_CLERR (errcode);
    priv->device_image = mem;
}
#else
static void
alloc_device_image (UfoBufferPrivate *priv)
{
    cl_image_format format;
    cl_mem_flags flags;
    cl_int err;
    gsize width, height, depth;
    cl_mem mem = NULL;

    g_assert ((priv->requisition.n_dims == 2) ||
              (priv->requisition.n_dims == 3));

    if (priv->device_image != NULL)
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->device_image));

    format.image_channel_order = CL_INTENSITY;
    format.image_channel_data_type = CL_FLOAT;

    flags = CL_MEM_READ_WRITE;
    width = priv->requisition.dims[0];
    height = priv->requisition.dims[1];
    depth = priv->requisition.dims[2];

    if (priv->requisition.n_dims == 2) {
        mem = clCreateImage2D (priv->context,
                               flags, &format,
                               width, height, 0,
                               NULL, &err);
    }
    else if (priv->requisition.n_dims == 3) {
        mem = clCreateImage3D (priv->context,
                               flags, &format,
                               width, height, depth, 0, 0,
                               NULL, &err);
    }

    UFO_RESOURCES_CHECK_CLERR (err);
    g_assert (mem != NULL);
    priv->device_image = mem;
}
#endif

/**
 * ufo_buffer_new:
 * @requisition: (in): size requisition
 * @context: (in) (allow-none): cl_context to use for creating the device array
 *
 * Create a new #UfoBuffer.
 *
 * Return value: A new #UfoBuffer with the given dimensions.
 */
UfoBuffer *
ufo_buffer_new (UfoRequisition *requisition,
                gpointer context)
{
    UfoBuffer *buffer;
    UfoBufferPrivate *priv;

    g_return_val_if_fail ((requisition->n_dims <= UFO_BUFFER_MAX_NDIMS) &&
                          (requisition->n_dims > 0), NULL);
    buffer = UFO_BUFFER (g_object_new (UFO_TYPE_BUFFER, NULL));
    priv = buffer->priv;
    priv->context = context;

    priv->size = compute_required_size (requisition);
    copy_requisition (requisition, &priv->requisition);

    return buffer;
}

/**
 * ufo_buffer_new_with_size:
 * @dims: (element-type guint64): size requisition
 * @context: (allow-none): cl_context to use for creating the device array
 *
 * Create a new #UfoBuffer with a list of dimensions.
 *
 * Return value: A new #UfoBuffer with the given dimensions.
 */
UfoBuffer *
ufo_buffer_new_with_size (GList *dims,
                          gpointer context)
{
    UfoRequisition req;

    req.n_dims = g_list_length (dims);
    g_assert (req.n_dims < 16);

    for (guint i = 0; i < req.n_dims; i++)
        req.dims[i] = (gsize) g_list_nth_data (dims, i);

    return ufo_buffer_new (&req, context);
}

/**
 * ufo_buffer_new_with_data:
 * @requisition: size requisition
 * @data: Pointer to host memory that will be used by
 * @context: OpenCL context for this buffer
 *
 * Create a new buffer using existing host memory.
 */
UfoBuffer *
ufo_buffer_new_with_data (UfoRequisition *requisition,
                          gpointer data,
                          gpointer context)
{
    UfoBuffer *buffer;
    UfoBufferPrivate *priv;

    g_return_val_if_fail ((requisition->n_dims <= UFO_BUFFER_MAX_NDIMS) &&
                          (requisition->n_dims > 0), NULL);

    buffer = ufo_buffer_new (requisition, context);
    priv = buffer->priv;

    priv->free = FALSE;
    priv->host_array = data;

    return buffer;
}

/**
 * ufo_buffer_get_size:
 * @buffer: A #UfoBuffer
 *
 * Get the number of bytes of raw data that is managed by the @buffer.
 *
 * Returns: The size of @buffer's data.
 */
gsize
ufo_buffer_get_size (UfoBuffer *buffer)
{
    g_return_val_if_fail (UFO_IS_BUFFER (buffer), 0);
    return buffer->priv->size;
}

static gsize
get_num_elements (UfoBufferPrivate *priv)
{
    gsize n = 1;

    for (guint i = 0; i < priv->requisition.n_dims; i++)
        n *= priv->requisition.dims[i];

    return n;
}

static void
set_region_from_requisition (size_t region[3],
                             UfoRequisition *requisition)
{
    region[0] = requisition->dims[0];
    region[1] = requisition->dims[1];

    if (requisition->n_dims == 3)
        region[2] = requisition->dims[2];
    else
        region[2] = 1;
}

static void
transfer_host_to_host (UfoBufferPrivate *src_priv,
                       UfoBufferPrivate *dst_priv,
                       cl_command_queue queue)
{
    g_memmove (dst_priv->host_array,
               src_priv->host_array,
               src_priv->size);
}

static void
transfer_host_to_device (UfoBufferPrivate *src_priv,
                         UfoBufferPrivate *dst_priv,
                         cl_command_queue queue)
{
    cl_int errcode;

    errcode = clEnqueueWriteBuffer (queue,
                                    dst_priv->device_array,
                                    CL_TRUE,
                                    0, src_priv->size,
                                    src_priv->host_array,
                                    0, NULL, NULL);

    UFO_RESOURCES_CHECK_CLERR (errcode);
}

static void
transfer_host_to_image (UfoBufferPrivate *src_priv,
                        UfoBufferPrivate *dst_priv,
                        cl_command_queue queue)
{
    cl_int errcode;
    cl_event event;
    size_t region[3];
    size_t origin[] = { 0, 0, 0 };

    set_region_from_requisition (region, &src_priv->requisition);

    errcode = clEnqueueWriteImage (queue,
                                   dst_priv->device_image,
                                   CL_TRUE,
                                   origin, region,
                                   0, 0,
                                   src_priv->host_array,
                                   0, NULL, &event);

    UFO_RESOURCES_CHECK_CLERR (errcode);
    UFO_RESOURCES_CHECK_CLERR (clWaitForEvents (1, &event));
    UFO_RESOURCES_CHECK_CLERR (clReleaseEvent (event));
}

static void
transfer_device_to_device (UfoBufferPrivate *src_priv,
                           UfoBufferPrivate *dst_priv,
                           cl_command_queue queue)
{
    cl_event event;
    cl_int errcode;

    errcode = clEnqueueCopyBuffer (queue,
                                   src_priv->device_array,
                                   dst_priv->device_array,
                                   0, 0,
                                   src_priv->size,
                                   0, NULL, &event);

    UFO_RESOURCES_CHECK_CLERR (errcode);
    UFO_RESOURCES_CHECK_CLERR (clWaitForEvents (1, &event));
    UFO_RESOURCES_CHECK_CLERR (clReleaseEvent (event));
}

static void
transfer_device_to_host (UfoBufferPrivate *src_priv,
                         UfoBufferPrivate *dst_priv,
                         cl_command_queue queue)
{
    cl_int errcode;

    errcode = clEnqueueReadBuffer (queue,
                                   src_priv->device_array,
                                   CL_TRUE,
                                   0, src_priv->size,
                                   dst_priv->host_array,
                                   0, NULL, NULL);

    UFO_RESOURCES_CHECK_CLERR (errcode);
}

static void
transfer_device_to_image (UfoBufferPrivate *src_priv,
                          UfoBufferPrivate *dst_priv,
                          cl_command_queue queue)
{
    cl_event event;
    cl_int errcode;
    size_t region[3];
    size_t origin[] = { 0, 0, 0 };

    set_region_from_requisition (region, &src_priv->requisition);

    errcode = clEnqueueCopyBufferToImage (queue,
                                          src_priv->device_array,
                                          dst_priv->device_image,
                                          0, origin, region,
                                          0, NULL, &event);

    UFO_RESOURCES_CHECK_CLERR (errcode);
    UFO_RESOURCES_CHECK_CLERR (clWaitForEvents (1, &event));
    UFO_RESOURCES_CHECK_CLERR (clReleaseEvent (event));
}

static void
transfer_image_to_image (UfoBufferPrivate *src_priv,
                         UfoBufferPrivate *dst_priv,
                         cl_command_queue queue)
{
    cl_event event;
    cl_int errcode;
    size_t region[3];
    size_t origin[] = { 0, 0, 0 };

    set_region_from_requisition (region, &src_priv->requisition);

    errcode = clEnqueueCopyImage (queue,
                                  src_priv->device_image,
                                  dst_priv->device_image,
                                  origin, origin, region,
                                  0, NULL, &event);

    UFO_RESOURCES_CHECK_CLERR (errcode);
    UFO_RESOURCES_CHECK_CLERR (clWaitForEvents (1, &event));
    UFO_RESOURCES_CHECK_CLERR (clReleaseEvent (event));
}

static void
transfer_image_to_host (UfoBufferPrivate *src_priv,
                        UfoBufferPrivate *dst_priv,
                        cl_command_queue queue)
{
    cl_int errcode;
    size_t region[3];
    size_t origin[] = { 0, 0, 0 };

    set_region_from_requisition (region, &src_priv->requisition);

    errcode = clEnqueueReadImage (queue,
                                  src_priv->device_image,
                                  CL_TRUE,
                                  origin, region,
                                  0, 0,
                                  dst_priv->host_array,
                                  0, NULL, NULL);

    UFO_RESOURCES_CHECK_CLERR (errcode);
}

static void
transfer_image_to_device (UfoBufferPrivate *src_priv,
                          UfoBufferPrivate *dst_priv,
                          cl_command_queue queue)
{
    cl_event event;
    cl_int errcode;
    size_t region[3];
    size_t origin[] = { 0, 0, 0 };

    set_region_from_requisition (region, &src_priv->requisition);

    errcode = clEnqueueCopyImageToBuffer (queue,
                                          src_priv->device_image,
                                          dst_priv->device_array,
                                          origin, region, 0,
                                          0, NULL, &event);

    UFO_RESOURCES_CHECK_CLERR (errcode);
    UFO_RESOURCES_CHECK_CLERR (clWaitForEvents (1, &event));
    UFO_RESOURCES_CHECK_CLERR (clReleaseEvent (event));
}

//----------------------------------------------------------------------------------------------------------------
/**
 * alloc_device_array_direct_gma
 * @priv: buffer to be created for directgma (not pinned)
 * this function create a buffer that can be used for directGMA, without pinning it on GPU memory
 */
static int
alloc_device_array_direct_gma (UfoBufferPrivate *priv)
{
    cl_int err;
    cl_mem mem;

    if (priv->device_array != NULL)
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->device_array));
    
    if(priv->size>128000000){
      g_printerr("the requested buffer size for directgma is too big");
      return 1;
    }
    
    if(priv->location != UFO_BUFFER_LOCATION_DEVICE_DIRECT_GMA){
      g_printerr("wrong buffer type for directGMA");
      return 2;
    }

    mem = clCreateBuffer (priv->context,
                          CL_MEM_BUS_ADDRESSABLE_AMD,
                          priv->size,
                          NULL, &err);

    UFO_RESOURCES_CHECK_CLERR (err);
    priv->device_array = mem;
    return 0;
}

/**
 * make_buffer_resident_amd
 * @priv: the buffer we want to open for directgma on the gpu
 * @command_queue: the command queue running
 *
 * this function opens the buffer for directgma on gpu, with size of priv
 */ 
static cl_bus_address_amd 
make_buffer_resident_amd(UfoBufferPrivate *priv, cl_command_queue command_queue,){
  clEnqueueMakeBuffersResidentAMD_fn clEnqueueMakeBuffersResidentAMD=NULL;
  gint err2=0;
  cl_bus_address_amd busaddress;
    
clEnqueueMakeBuffersResidentAMD = (clEnqueueMakeBuffersResidentAMD_fn) clGetExtensionFunctionAddressForPlatform(SelectedPlatform,"clEnqueueMakeBuffersResidentAMD");
 if(clEnqueueMakeBuffersResidentAMD == NULL) {
   g_printerr("impossible to get the clEnqueueMakeBuffersResdidentAMD function");
   return NULL;
 }
  
  err2=clEnqueueMakeBuffersResidentAMD(command_queue,1,priv->device_array, CL_TRUE, &busaddress,0,0,0);
  UFO_RESOURCES_CHECK_CLERR (err2);

  return busaddress;
}

/**
 * ufo_direct_gma_address_share:
 * @busadress: struct #cl_bus_address_amd returned by ufo_make_buffer_resident_amd
 *
 * share the beginning pcibus address of the buffer for directGMA on the GPU
 */
int
ufo_direct_gma_address_share(cl_bus_address_amd busadress){
  glong bus_gpu;
  gint key=987654;

    shmid=shmget(key, sizeof(glong), IPC_CREAT| IPC_EXCL | 0666);
    if(errno==EEXIST){
        shmid=shmget(key,sizeof(glong),0666);
        if(shmid<0) {
	  g_printerr("impossible to get a shm1,errno %i",errno); /**< exit*/
	  return 1;
	}
    }else if(shmid<0){
      g_printerr("impossible to get a shm2, errno %i",errno); /**exit*/
      return 1;
    }      

    bus_gpu=shmat(shmid,(void*)0,0);
    if(bus_gpu==(glong*)(-1)){
      g_printerr("error getting shmat");
      return 2;
    }

    bus_gpu=busadress.surface_bus_address; /* we will care about offset and alignment in fpga part*/
    return 0;
}
  
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * ufo_buffer_copy:
 * @src: Source #UfoBuffer
 * @dst: Destination #UfoBuffer
 *
 * Copy contents of @src to @dst. The final memory location is determined by the
 * destination buffer.
 */
void
ufo_buffer_copy (UfoBuffer *src, UfoBuffer *dst)
{
    typedef void (*TransferFunc) (UfoBufferPrivate *, UfoBufferPrivate *, cl_command_queue);
    typedef void (*AllocFunc) (UfoBufferPrivate *priv);

    UfoBufferPrivate *spriv;
    UfoBufferPrivate *dpriv;
    cl_command_queue queue;

    TransferFunc transfer[3][3] = {
        { transfer_host_to_host, transfer_host_to_device, transfer_host_to_image },
        { transfer_device_to_host, transfer_device_to_device, transfer_device_to_image },
        { transfer_image_to_host, transfer_image_to_device, transfer_image_to_image }
    };

    AllocFunc alloc[3] = { alloc_host_mem, alloc_device_array, alloc_device_image };

    g_return_if_fail (UFO_IS_BUFFER (src) && UFO_IS_BUFFER (dst));

    if (ufo_buffer_cmp_dimensions (dst, &src->priv->requisition) != 0)
        ufo_buffer_resize (dst, &src->priv->requisition);

    spriv = src->priv;
    dpriv = dst->priv;
    queue = spriv->last_queue != NULL ? spriv->last_queue : dpriv->last_queue;

    if (spriv->location == UFO_BUFFER_LOCATION_INVALID) {
        alloc_host_mem (spriv);
        spriv->location = UFO_BUFFER_LOCATION_HOST;
    }

    if (dpriv->location == UFO_BUFFER_LOCATION_INVALID ||
        (!dpriv->host_array && !dpriv->device_array && !dpriv->device_image)) {
        alloc[spriv->location](dpriv);
        dpriv->location = spriv->location;
    }

    transfer[spriv->location][dpriv->location](spriv, dpriv, queue);
    dpriv->last_queue = queue;
}

/**
 * ufo_buffer_dup:
 * @buffer: A #UfoBuffer
 *
 * Create a new buffer with the same requisition as @buffer. Note, that this is
 * not a copy of @buffer!
 *
 * Returns: (transfer full): A #UfoBuffer with the same size as @buffer.
 */
UfoBuffer *
ufo_buffer_dup (UfoBuffer *buffer)
{
    UfoBuffer *copy;
    UfoRequisition requisition;

    ufo_buffer_get_requisition (buffer, &requisition);
    copy = ufo_buffer_new (&requisition, buffer->priv->context);
    return copy;
}

/**
 * ufo_buffer_resize:
 * @buffer: A #UfoBuffer
 * @requisition: A #UfoRequisition structure
 *
 * Resize an existing buffer. If the new requisition has the same size as
 * before, resizing is a no-op.
 *
 * Since: 0.2
 */
void
ufo_buffer_resize (UfoBuffer *buffer,
                   UfoRequisition *requisition)
{
    UfoBufferPrivate *priv;

    g_return_if_fail (UFO_IS_BUFFER (buffer));

    if (ufo_buffer_cmp_dimensions (buffer, requisition) == 0)
        return;

    priv = UFO_BUFFER_GET_PRIVATE (buffer);

    if (priv->host_array != NULL && priv->free) {
        g_free (priv->host_array);
        priv->host_array = NULL;
    }

    if (priv->device_array != NULL) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->device_array));
        priv->device_array = NULL;
    }

    if (priv->device_image != NULL) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (priv->device_image));
        priv->device_image = NULL;
    }

    priv->size = compute_required_size (requisition);
    copy_requisition (requisition, &priv->requisition);
}

/**
 * ufo_buffer_cmp_dimensions:
 * @buffer: A #UfoBuffer
 * @requisition: #UfoRequisition
 *
 * Compare the size of @buffer with a given @requisition.
 *
 * Returns: value < 0, 0 or > 0 if requisition is smaller, equal or larger.
 */
gint
ufo_buffer_cmp_dimensions (UfoBuffer *buffer,
                           UfoRequisition *requisition)
{
    UfoBufferPrivate *priv;
    gint result;

    g_return_val_if_fail (UFO_IS_BUFFER(buffer), FALSE);

    priv = buffer->priv;
    result = 0;

    for (guint i = 0; i < priv->requisition.n_dims; i++) {
        gint req_dim = (gint) requisition->dims[i];
        gint host_dim = (gint) priv->requisition.dims[i];
        result += req_dim - host_dim;
    }

    return result;
}

/**
 * ufo_buffer_get_requisition:
 * @buffer: A #UfoBuffer
 * @requisition: (out): A location to store the requisition of @buffer
 *
 * Return the size of @buffer.
 */
void
ufo_buffer_get_requisition (UfoBuffer *buffer,
                            UfoRequisition *requisition)
{
    UfoBufferPrivate *priv;

    g_return_if_fail (UFO_IS_BUFFER (buffer) && (requisition != NULL));
    priv = buffer->priv;

    copy_requisition (&priv->requisition, requisition);
}

static void
update_last_queue (UfoBufferPrivate *priv,
                   cl_command_queue queue)
{
    if (queue != NULL)
        priv->last_queue = queue;
}

static void
update_location (UfoBufferPrivate *priv,
                 UfoBufferLocation new_location)
{
    priv->last_location = priv->location;
    priv->location = new_location;
}

/**
 * ufo_buffer_set_host_array:
 * @buffer: A #UfoBuffer
 * @array: (type gulong): A pointer to a float array with suitable size.
 * @free_data: %TRUE if @buffer is supposed to clean up the host array.
 *
 * Use this function to set a host array with a user-provided memory buffer.
 * This is useful to expose private data of a generator filter to a subsequent
 * consumer. Note, that the buffer *must* have an appropriate size.
 */
void
ufo_buffer_set_host_array (UfoBuffer *buffer, gpointer array, gboolean free_data)
{
    UfoBufferPrivate *priv;

    g_return_if_fail (UFO_IS_BUFFER (buffer));

    priv = buffer->priv;

    if (priv->free)
        g_free (priv->host_array);

    priv->free = free_data;
    priv->host_array = array;

    update_location (priv, UFO_BUFFER_LOCATION_HOST);
}

/**
 * ufo_buffer_get_host_array:
 * @buffer: A #UfoBuffer.
 * @cmd_queue: (allow-none): A cl_command_queue object or %NULL.
 *
 * Returns a flat C-array containing the raw float data.
 *
 * Returns: Float array.
 */
gfloat *
ufo_buffer_get_host_array (UfoBuffer *buffer, gpointer cmd_queue)
{
    UfoBufferPrivate *priv;

    g_return_val_if_fail (UFO_IS_BUFFER (buffer), NULL);
    priv = buffer->priv;

    update_last_queue (priv, cmd_queue);

    if (priv->host_array == NULL)
        alloc_host_mem (priv);

    if (priv->location == UFO_BUFFER_LOCATION_DEVICE && priv->device_array)
        transfer_device_to_host (priv, priv, priv->last_queue);

    if (priv->location == UFO_BUFFER_LOCATION_DEVICE_IMAGE && priv->device_image)
        transfer_image_to_host (priv, priv, priv->last_queue);

    update_location (priv, UFO_BUFFER_LOCATION_HOST);

    return priv->host_array;
}

/**
 * ufo_buffer_get_device_array:
 * @buffer: A #UfoBuffer.
 * @cmd_queue: (allow-none): A cl_command_queue object or %NULL.
 *
 * Return the current cl_mem object of @buffer. If the data is not yet in device
 * memory, it is transfered via @cmd_queue to the object. If @cmd_queue is %NULL
 * @cmd_queue, the last used command queue is used.
 *
 * Returns: (transfer none): A cl_mem object associated with @buffer.
 */
gpointer
ufo_buffer_get_device_array (UfoBuffer *buffer, gpointer cmd_queue)
{
    UfoBufferPrivate *priv;

    g_return_val_if_fail (UFO_IS_BUFFER (buffer), NULL);
    priv = buffer->priv;

    update_last_queue (priv, cmd_queue);

    if (priv->device_array == NULL && priv->location!= UFO_BUFFER_LOCATION_DEVICE_DIRECT_GMA)
        alloc_device_array (priv);
    
    if (priv->device_array == NULL && priv->location== UFO_BUFFER_LOCATION_DEVICE_DIRECT_GMA){
        alloc_device_array_direct_gma (priv);
        make_buffer_resident_amd(priv);
    }

    if (priv->location == UFO_BUFFER_LOCATION_HOST && priv->host_array)
        transfer_host_to_device (priv, priv, priv->last_queue);

    if (priv->location == UFO_BUFFER_LOCATION_DEVICE_IMAGE && priv->device_array)
        transfer_image_to_device (priv, priv, priv->last_queue);

    update_location (priv, UFO_BUFFER_LOCATION_DEVICE);

    return priv->device_array;
}

/**
 * ufo_buffer_get_device_array_with_offset:
 * @buffer: A #UfoBuffer
 * @cmd_queue: (allow-none): A cl_command_queue object or %NULL
 * @offset: Offset in bytes from the original buffer
 *
 * Creates a new cl_mem object with the given offset and a size that is the
 * original size minus the offset.
 *
 * Returns: (transfer none): A cl_mem sub buffer of the original data.
 */
gpointer
ufo_buffer_get_device_array_with_offset (UfoBuffer *buffer,
                                         gpointer cmd_queue,
                                         gsize offset)
{
    UfoBufferPrivate *priv;
    cl_mem device_array;
    cl_mem sub_buffer;
    cl_mem_flags mem_flags;
    cl_buffer_region region;
    cl_int errcode;
    size_t size;

    g_return_val_if_fail (UFO_IS_BUFFER (buffer), NULL);
    priv = buffer->priv;

    device_array = ufo_buffer_get_device_array (buffer, cmd_queue);

    UFO_RESOURCES_CHECK_CLERR (clGetMemObjectInfo (device_array, CL_MEM_FLAGS,
                                                   sizeof (cl_mem_flags),
                                                   &mem_flags, NULL));

    UFO_RESOURCES_CHECK_CLERR (clGetMemObjectInfo (device_array, CL_MEM_SIZE,
                                                   sizeof (size_t),
                                                   &size, NULL));

    region.origin = offset;
    region.size = size - offset;

    sub_buffer = clCreateSubBuffer (device_array, mem_flags, CL_BUFFER_CREATE_TYPE_REGION,
                                    &region, &errcode);

    UFO_RESOURCES_CHECK_CLERR (errcode);
    priv->sub_device_arrays = g_list_append (priv->sub_device_arrays, sub_buffer);
    return sub_buffer;
}

/**
 * ufo_buffer_get_device_array_view:
 * @buffer: A #UfoBuffer
 * @cmd_queue: A cl_command_queue object
 * @region: A #UfoRegion specifying the view of the sub buffer
 *
 * This method creates a new memory buffer that must be freed by the user.
 * Moreover, the original @buffer is kept intact.
 *
 * Returns: (transfer full): A newly allocated cl_mem that the user must release
 * himself with clReleaseMemObject().
 */
gpointer
ufo_buffer_get_device_array_view (UfoBuffer *buffer,
                                  gpointer cmd_queue,
                                  UfoRegion *region)
{
    UfoBufferPrivate *priv;
    gsize size;
    gsize src_row_pitch;
    gsize src_slice_pitch;
    gsize dst_row_pitch;
    gsize dst_slice_pitch;
    cl_mem mem;
    cl_int errcode;
    gsize dst_origin[] = {0, 0, 0};

    g_return_val_if_fail (UFO_IS_BUFFER (buffer), NULL);
    priv = buffer->priv;

    if (region->origin[0] + region->size[0] > priv->requisition.dims[0] ||
        (priv->requisition.n_dims == 2 && region->origin[1] + region->size[1] > priv->requisition.dims[1]) ||
        (priv->requisition.n_dims == 3 && region->origin[2] + region->size[2] > priv->requisition.dims[2])) {
        g_error ("Requested view exceeds buffer size");
        return NULL;
    }

    update_last_queue (priv, cmd_queue);

    size = region->size[0] * region->size[1] * region->size[2] * sizeof(float);
    src_row_pitch = sizeof(float) * priv->requisition.dims[0];
    src_slice_pitch = sizeof(float) * priv->requisition.dims[1];
    dst_row_pitch = sizeof(float) * region->size[0];
    dst_slice_pitch = sizeof(float) * region->size[1];

    mem = clCreateBuffer (priv->context, CL_MEM_READ_WRITE, size, NULL, &errcode);
    UFO_RESOURCES_CHECK_CLERR (errcode);

    if (priv->location == UFO_BUFFER_LOCATION_HOST && priv->host_array) {
        if (priv->requisition.n_dims == 1) {
            UFO_RESOURCES_CHECK_CLERR (clEnqueueWriteBuffer (cmd_queue, mem, CL_TRUE,
                                                             region->origin[0] * sizeof (float), size,
                                                             priv->host_array,
                                                             0, NULL, NULL));
        }
        else if (priv->requisition.n_dims == 2) {
            if (region->size[0] == priv->requisition.dims[0]) {
                /* If our region is as wide as our buffer, we can simply copy
                 * with a fixed offset */
                gsize offset;

                offset = region->origin[1] * src_row_pitch;
                UFO_RESOURCES_CHECK_CLERR (clEnqueueWriteBuffer (cmd_queue, mem, CL_TRUE,
                                                                 0, size,
                                                                 ((gchar *) priv->host_array) + offset,
                                                                 0, NULL, NULL));
            }
            else {
                gchar *tmp;
                gchar *dst;
                gchar *src;
                guint n_rows;

                n_rows = region->size[1];
                tmp = dst = g_malloc0 (size);
                src = ((gchar *) priv->host_array) + region->origin[1] * src_row_pitch + region->origin[0] * sizeof (float);

                for (guint y = 0; y < n_rows; y++) {
                    memcpy (dst, src , dst_row_pitch);
                    dst += dst_row_pitch;
                    src += src_row_pitch;
                }

                UFO_RESOURCES_CHECK_CLERR (clEnqueueWriteBuffer (cmd_queue, mem, CL_TRUE,
                                                                 0, size, tmp,
                                                                 0, NULL, NULL));
                g_free (tmp);
            }
        }
        else {
            g_warning ("Dimensions >= 3 not supported yet");
        }
    }

    if (priv->location == UFO_BUFFER_LOCATION_DEVICE_IMAGE && priv->device_array) {
        cl_event event;

        UFO_RESOURCES_CHECK_CLERR (clEnqueueCopyBufferRect (cmd_queue,
                                                            priv->device_array, mem,
                                                            region->origin, dst_origin,
                                                            region->size,
                                                            src_row_pitch, src_slice_pitch,
                                                            dst_row_pitch, dst_slice_pitch,
                                                            0, NULL, &event));
        UFO_RESOURCES_CHECK_CLERR (clWaitForEvents (1, &event));
        UFO_RESOURCES_CHECK_CLERR (clReleaseEvent (event));
    }

    return mem;
}

/**
 * ufo_buffer_get_device_image:
 * @buffer: A #UfoBuffer.
 * @cmd_queue: (allow-none): A cl_command_queue object or %NULL.
 *
 * Return the current cl_mem image object of @buffer. If the data is not yet in
 * device memory, it is transfered via @cmd_queue to the object. If @cmd_queue
 * is %NULL @cmd_queue, the last used command queue is used.
 *
 * Returns: (transfer none): A cl_mem image object associated with @buffer.
 */
gpointer
ufo_buffer_get_device_image (UfoBuffer *buffer,
                             gpointer cmd_queue)
{
    UfoBufferPrivate *priv;

    g_return_val_if_fail (UFO_IS_BUFFER (buffer), NULL);
    priv = buffer->priv;

    update_last_queue (priv, cmd_queue);

    if (priv->device_image == NULL)
        alloc_device_image (priv);

    if (priv->location == UFO_BUFFER_LOCATION_HOST && priv->host_array)
        transfer_host_to_image (priv, priv, priv->last_queue);

    if (priv->location == UFO_BUFFER_LOCATION_DEVICE && priv->device_array)
        transfer_device_to_image (priv, priv, priv->last_queue);

    update_location (priv, UFO_BUFFER_LOCATION_DEVICE_IMAGE);

    return priv->device_image;
}

/**
 * ufo_buffer_get_location:
 * @buffer: A #UfoBuffer
 *
 * Return current location of data backed by @buffer.
 */
UfoBufferLocation
ufo_buffer_get_location (UfoBuffer *buffer)
{
    g_return_val_if_fail (UFO_IS_BUFFER (buffer), UFO_BUFFER_LOCATION_INVALID);
    return buffer->priv->location;
}

/**
 * ufo_buffer_discard_location:
 * @buffer: A #UfoBuffer
 *
 * Discard the current and use the last location without copying to it first.
 */
void
ufo_buffer_discard_location (UfoBuffer *buffer)
{
    g_return_if_fail (UFO_IS_BUFFER (buffer));
    buffer->priv->location = buffer->priv->last_location;
}

static void
convert_data (UfoBufferPrivate *priv,
              gconstpointer data,
              UfoBufferDepth depth)
{
    gint n_pixels;
    gfloat *dst;

    n_pixels = (gint) (priv->size / 4);
    dst = priv->host_array;

    /* To save a memory allocation and several copies, we process data from back
     * to front. This is possible if src bit depth is at most half as wide as
     * the 32-bit target buffer. The processor cache should not be a
     * problem. */
    if (depth == UFO_BUFFER_DEPTH_8U) {
        const guint8 *src = (const guint8 *) data;

        for (gint i = (n_pixels - 1); i >= 0; i--)
            dst[i] = ((gfloat) src[i]);
    }
    else if (depth == UFO_BUFFER_DEPTH_16U) {
        const guint16 *src = (const guint16 *) data;

        for (gint i = (n_pixels - 1); i >= 0; i--)
            dst[i] = ((gfloat) src[i]);
    }
    else if (depth == UFO_BUFFER_DEPTH_16S) {
        const gint16 *src = (const gint16 *) data;

        for (gint i = (n_pixels - 1); i >= 0; i--)
            dst[i] = ((gfloat) src[i]);
    }
    else if (depth == UFO_BUFFER_DEPTH_32S) {
        const gint32 *src = (const gint32 *) data;

        for (gint i = (n_pixels - 1); i >= 0; i--)
            dst[i] = ((gfloat) src[i]);
    }
    else if (depth == UFO_BUFFER_DEPTH_32U) {
        const guint32 *src = (const guint32 *) data;

        for (gint i = (n_pixels - 1); i >= 0; i--)
            dst[i] = ((gfloat) src[i]);
    }
}

/**
 * ufo_buffer_convert:
 * @buffer: A #UfoBuffer
 * @depth: Source bit depth of host data
 *
 * Convert host data according to its @depth to the internal 32-bit floating
 * point representation.
 */
void
ufo_buffer_convert (UfoBuffer *buffer,
                    UfoBufferDepth depth)
{
    UfoBufferPrivate *priv;

    g_return_if_fail (UFO_IS_BUFFER (buffer));
    priv = buffer->priv;

    if (priv->host_array != NULL)
        convert_data (priv, priv->host_array, depth);
}

/**
 * ufo_buffer_convert_from_data:
 * @buffer: A #UfoBuffer
 * @data: Pointer to data that should be converted
 * @depth: Source bit depth of host data
 *
 * Convert @data according from @depth to the internal 32-bit floating
 * point representation.
 *
 * Note: @data must provide as many bytes as the buffer was initialized with.
 */
void
ufo_buffer_convert_from_data (UfoBuffer *buffer,
                              gconstpointer data,
                              UfoBufferDepth depth)
{
    UfoBufferPrivate *priv;

    g_return_if_fail (UFO_IS_BUFFER (buffer));
    priv = buffer->priv;

    if (priv->host_array == NULL)
        alloc_host_mem (priv);

    convert_data (priv, data, depth);
}

/**
 * ufo_buffer_get_metadata:
 * @buffer: A #UfoBuffer
 * @name: Name of the associated meta data
 *
 * Retrieve meta data.
 *
 * Returns: previously defined metadata #GValue for this buffer.
 */
GValue *
ufo_buffer_get_metadata (UfoBuffer *buffer,
                         const gchar *name)
{
    g_return_val_if_fail (UFO_IS_BUFFER (buffer), NULL);
    return g_hash_table_lookup (buffer->priv->metadata, name);
}

/**
 * ufo_buffer_set_metadata:
 * @buffer: A #UfoBuffer
 * @name: Name of the associated meta data
 * @value: #GValue of the meta data
 *
 * Associates a key-value pair with @buffer.
 */
void
ufo_buffer_set_metadata (UfoBuffer *buffer,
                         const gchar *name,
                         GValue *value)
{
    UfoBufferPrivate *priv;
    GValue *old;
    GValue *new;

    g_return_if_fail (UFO_IS_BUFFER (buffer));
    priv = buffer->priv;

    old = g_hash_table_lookup (priv->metadata, name);

    if (old != NULL) {
        g_value_unset (old);
    }

    new = g_malloc0 (sizeof (GValue));
    g_value_init (new, G_VALUE_TYPE ((GValue *) value));
    g_value_copy (value, new);

    g_hash_table_replace (priv->metadata, g_strdup (name), new);
}

/**
 * ufo_buffer_copy_metadata:
 * @src: Source buffer
 * @dst: Destination buffer
 *
 * Copies meta data content from @src to @dst.
 */
void
ufo_buffer_copy_metadata (UfoBuffer *src,
                          UfoBuffer *dst)
{
    UfoBufferPrivate *priv;
    GList *keys;
    GList *it;

    g_return_if_fail (UFO_IS_BUFFER (src) && UFO_IS_BUFFER (dst));
    priv = src->priv;
    keys = g_hash_table_get_keys (priv->metadata);

    g_list_for (keys, it) {
        GValue *value;
        const gchar *name = (const gchar *) it->data;

        value = ufo_buffer_get_metadata (src, name);
        ufo_buffer_set_metadata (dst, name, value);
    }

    g_list_free (keys);
}

/**
 * ufo_buffer_get_metadata_keys:
 * @buffer: A #UfoBuffer
 *
 * Get all meta data keys associated with @buffer.
 *
 * Returns: (element-type utf8) (transfer container): A #GList with C strings.
 * The content of the list owned by the buffer and should not be modified or
 * freed. Use #g_list_free() when done using the list.
 */
GList *
ufo_buffer_get_metadata_keys (UfoBuffer *buffer)
{
    g_return_val_if_fail (UFO_IS_BUFFER (buffer), NULL);
    return g_hash_table_get_keys (buffer->priv->metadata);
}

/**
 * ufo_buffer_max:
 * @buffer: A #UfoBuffer
 * @cmd_queue: An OpenCL command queue or %NULL
 *
 * Return the maximum value of @buffer.
 *
 * Returns: The maximum found.
 */
gfloat
ufo_buffer_max (UfoBuffer *buffer,
                gpointer cmd_queue)
{
    UfoBufferPrivate *priv;
    gsize n;
    gfloat max = -G_MAXFLOAT;

    g_return_val_if_fail (UFO_IS_BUFFER (buffer), 0.0f);

    priv = buffer->priv;

    if (priv->location != UFO_BUFFER_LOCATION_HOST) {
        g_warning ("max() not supported for non-host buffers");
        return 0.0f;
    }

    n = get_num_elements (priv);

    for (gsize i = 0; i < n; i++) {
        if (priv->host_array[i] > max)
            max = priv->host_array[i];
    }

    return max;
}

/**
 * ufo_buffer_min:
 * @buffer: A #UfoBuffer
 * @cmd_queue: An OpenCL command queue or %NULL
 *
 * Return the minimum value of @buffer.
 *
 * Returns: The minimum found.
 */
gfloat
ufo_buffer_min (UfoBuffer *buffer,
                gpointer cmd_queue)
{
    UfoBufferPrivate *priv;
    gsize n;
    gfloat min = G_MAXFLOAT;

    g_return_val_if_fail (UFO_IS_BUFFER (buffer), 0.0f);

    priv = buffer->priv;

    if (priv->location != UFO_BUFFER_LOCATION_HOST) {
        g_warning ("min() not supported for non-host buffers");
        return 0.0f;
    }

    n = get_num_elements (priv);

    for (gsize i = 0; i < n; i++) {
        if (priv->host_array[i] < min)
            min = priv->host_array[i];
    }

    return min;
}

/**
 * ufo_buffer_param_spec:
 * @name: canonical name of the property specified
 * @nick: nick name for the property specified
 * @blurb: description of the property specified
 * @default_value: default value for the property specified
 * @flags: flags for the property specified
 *
 * Creates a new #UfoBufferParamSpec instance specifying a #UFO_TYPE_BUFFER
 * property.
 *
 * Returns: (transfer none): a newly created parameter specification
 *
 * @see g_param_spec_internal() for details on property names.
 */
GParamSpec *
ufo_buffer_param_spec(const gchar *name, const gchar *nick, const gchar *blurb, UfoBuffer *default_value, GParamFlags flags)
{
    UfoBufferParamSpec *bspec;

    bspec = g_param_spec_internal(UFO_TYPE_PARAM_BUFFER,
            name, nick, blurb, flags);

    return G_PARAM_SPEC(bspec);
}

static void
free_cl_mem (cl_mem *mem)
{
    g_assert (mem != NULL);

    if (*mem != NULL) {
        UFO_RESOURCES_CHECK_CLERR (clReleaseMemObject (*mem));
        *mem = NULL;
    }
}

static void
ufo_buffer_finalize (GObject *gobject)
{
    GList *it;
    UfoBuffer *buffer = UFO_BUFFER (gobject);
    UfoBufferPrivate *priv = UFO_BUFFER_GET_PRIVATE (buffer);

    if (priv->free)
        g_free (priv->host_array);

    priv->host_array = NULL;

    g_list_for (priv->sub_device_arrays, it) {
        free_cl_mem ((cl_mem *) &it->data);
    }

    free_cl_mem (&priv->device_array);
    free_cl_mem (&priv->device_image);

    g_hash_table_destroy (priv->metadata);

    G_OBJECT_CLASS(ufo_buffer_parent_class)->finalize(gobject);
}

static void
ufo_buffer_class_init (UfoBufferClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = ufo_buffer_finalize;

    g_type_class_add_private(klass, sizeof(UfoBufferPrivate));
}

static void
ufo_buffer_init (UfoBuffer *buffer)
{
    UfoBufferPrivate *priv;
    buffer->priv = priv = UFO_BUFFER_GET_PRIVATE(buffer);
    priv->last_queue = NULL;
    priv->device_array = NULL;
    priv->device_image = NULL;
    priv->host_array = NULL;
    priv->free = TRUE;

    priv->location = UFO_BUFFER_LOCATION_INVALID;
    priv->last_location = UFO_BUFFER_LOCATION_INVALID;
    priv->requisition.n_dims = 0;
    priv->metadata = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    priv->sub_device_arrays = NULL;
}

static void
ufo_buffer_param_init (GParamSpec *pspec)
{
    UfoBufferParamSpec *bspec = UFO_BUFFER_PARAM_SPEC(pspec);

    bspec->default_value = NULL;
}

static void
ufo_buffer_param_set_default (GParamSpec *pspec, GValue *value)
{
    UfoBufferParamSpec *bspec = UFO_BUFFER_PARAM_SPEC(pspec);

    bspec->default_value = NULL;
    g_value_unset(value);
}

GType
ufo_buffer_param_get_type()
{
    static GType type = 0;

    if (type == 0) {
        GParamSpecTypeInfo pspec_info = {
            sizeof(UfoBufferParamSpec),     /* instance_size */
            16,                             /* n_preallocs */
            ufo_buffer_param_init,          /* instance_init */
            0,                              /* value_type */
            NULL,                           /* finalize */
            ufo_buffer_param_set_default,   /* value_set_default */
            NULL,                           /* value_validate */
            NULL,                           /* values_cmp */
        };
        pspec_info.value_type = UFO_TYPE_BUFFER;
        type = g_param_type_register_static(g_intern_static_string("UfoBufferParam"), &pspec_info);
        g_assert(type == UFO_TYPE_PARAM_BUFFER);
    }

    return type;
}
