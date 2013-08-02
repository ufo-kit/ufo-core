#include <math.h>
#include <ufo/ufoart.h>
#include <ufo/ufo-basic-ops.h>
#define OPS_FILENAME "ufo-basic-ops.cl"

cl_event
operation (const gchar *kernel_name, 
           UfoBuffer *arg1,
           UfoBuffer *arg2,
           UfoBuffer *out,
           UfoResources *resources,
           cl_command_queue command_queue);

cl_event
operation2 (const gchar *kernel_name, 
            UfoBuffer *arg1,
            UfoBuffer *arg2,
            gfloat modifier,
            UfoBuffer *out,
            UfoResources *resources,
            cl_command_queue command_queue);


cl_event
op_set (UfoBuffer *arg, 
        gfloat value,
        UfoResources *resources,
        cl_command_queue command_queue)
{
  cl_event event;
  GError *error = NULL;
  UfoRequisition requisition;
  ufo_buffer_get_requisition (arg, &requisition);
  cl_mem d_arg = ufo_buffer_get_device_image (arg, command_queue);
  cl_kernel kernel = ufo_resources_get_kernel (resources, OPS_FILENAME, "operation_set", &error);
  if (error) {
    g_error (error->message);
    return NULL;
  }
  
  UFO_CHECK_CLERR (clSetKernelArg(kernel, 0, sizeof(void *), (void *) &d_arg));
  UFO_CHECK_CLERR (clSetKernelArg(kernel, 1, sizeof(gfloat), (void *) &value));
  UFO_CHECK_CLERR (clEnqueueNDRangeKernel(command_queue,
                                          kernel,
                                          requisition.n_dims,
                                          NULL,
                                          requisition.dims,
                                          NULL,  
                                          0, NULL, &event));
  return event;
}

cl_event
op_inv (UfoBuffer *arg, 
        UfoResources *resources,
        cl_command_queue command_queue)
{
  cl_event event;
  GError *error = NULL;
  UfoRequisition requisition;
  ufo_buffer_get_requisition (arg, &requisition);

  cl_mem d_arg = ufo_buffer_get_device_image (arg, command_queue);
  cl_kernel kernel = ufo_resources_get_kernel (resources, OPS_FILENAME, "operation_inv", &error);
  if (error) {
    g_error (error->message);
    return NULL;
  }

  UFO_CHECK_CLERR (clSetKernelArg(kernel, 0, sizeof(void *), (void *) &d_arg));
  UFO_CHECK_CLERR (clSetKernelArg(kernel, 1, sizeof(void *), (void *) &d_arg));
  UFO_CHECK_CLERR (clEnqueueNDRangeKernel(command_queue,
                                          kernel,
                                          requisition.n_dims,
                                          NULL,
                                          requisition.dims,
                                          NULL,  
                                          0, NULL, &event));
  return event;
}

cl_event
op_mul (UfoBuffer *arg1,
        UfoBuffer *arg2,
        UfoBuffer *out,
        UfoResources *resources,
        cl_command_queue command_queue)
{
  return operation ("operation_mul", arg1, arg2, out, resources, command_queue);
}

cl_event
op_add (UfoBuffer *arg1,
        UfoBuffer *arg2,
        UfoBuffer *out,
        UfoResources *resources,
        cl_command_queue command_queue)
{
  return operation ("operation_add", arg1, arg2, out, resources, command_queue);
}

cl_event
op_add2 (UfoBuffer *arg1,
         UfoBuffer *arg2,
         gfloat modifier,
         UfoBuffer *out,
         UfoResources *resources,
         cl_command_queue command_queue)
{
  return operation2 ("operation_add2", arg1, arg2, modifier, out, resources, command_queue);
}

cl_event
op_deduction (UfoBuffer *arg1,
              UfoBuffer *arg2,
              UfoBuffer *out,
              UfoResources *resources,
              cl_command_queue command_queue)
{ 
  return operation ("operation_deduction", arg1, arg2, out, resources, command_queue);
}

cl_event
op_deduction2 (UfoBuffer *arg1,
               UfoBuffer *arg2,
               gfloat modifier,
               UfoBuffer *out,
               UfoResources *resources,
               cl_command_queue command_queue)
{ 
  return operation2 ("operation_deduction2", arg1, arg2, modifier, out, resources, command_queue);
}

cl_event
op_mulRows (UfoBuffer *arg1,
            UfoBuffer *arg2,
            UfoBuffer *out, 
            guint offset,
            guint n,
            UfoResources *resources,
            cl_command_queue command_queue)
{
  cl_event event;
  GError *error = NULL;
  UfoRequisition arg1_requisition, arg2_requisition, out_requisition;
  ufo_buffer_get_requisition (arg1, &arg1_requisition);
  ufo_buffer_get_requisition (arg2, &arg2_requisition);
  ufo_buffer_get_requisition (out, &out_requisition);

  if (arg1_requisition.dims[0] != arg2_requisition.dims[0] ||
      arg1_requisition.dims[0] != out_requisition.dims[0]) {
    g_error ("Number of columns is different.");
    return NULL;
  }

  if(arg1_requisition.dims[1] < offset + n ||
     arg2_requisition.dims[1] < offset + n ||
     out_requisition.dims[1] < offset + n) {
    g_error ("Rows are not enough.");
    return NULL;
  }

  cl_mem d_arg1 = ufo_buffer_get_device_image (arg1, command_queue);
  cl_mem d_arg2 = ufo_buffer_get_device_image (arg2, command_queue);
  cl_mem d_out  = ufo_buffer_get_device_image (out, command_queue);
  cl_kernel kernel = ufo_resources_get_kernel (resources, OPS_FILENAME, "op_mulRows", &error);

  UFO_CHECK_CLERR (clSetKernelArg(kernel, 0, sizeof(void *), (void *) &d_arg1));
  UFO_CHECK_CLERR (clSetKernelArg(kernel, 1, sizeof(void *), (void *) &d_arg2));
  UFO_CHECK_CLERR (clSetKernelArg(kernel, 2, sizeof(void *), (void *) &d_out));
  UFO_CHECK_CLERR (clSetKernelArg(kernel, 3, sizeof(unsigned int), (void *) &offset));

  UfoRequisition operation_requisition = out_requisition;
  operation_requisition.dims[1] = n;

  UFO_CHECK_CLERR (clEnqueueNDRangeKernel(command_queue,
                                          kernel,
                                          operation_requisition.n_dims,
                                          NULL,
                                          operation_requisition.dims,
                                          NULL,  
                                          0, NULL, &event));

  return event;
}

cl_event
operation (const gchar *kernel_name, 
           UfoBuffer *arg1,
           UfoBuffer *arg2,
           UfoBuffer *out,
           UfoResources *resources,
           cl_command_queue command_queue)
{
  cl_event event;
  GError *error = NULL;
  UfoRequisition arg1_requisition, arg2_requisition, out_requisition;
  ufo_buffer_get_requisition (arg1, &arg1_requisition);
  ufo_buffer_get_requisition (arg2, &arg2_requisition);
  ufo_buffer_get_requisition (out, &out_requisition);

  if ((arg1_requisition.dims[0] != arg2_requisition.dims[0] && 
       arg1_requisition.dims[0] != out_requisition.dims[0]) ||
      (arg1_requisition.dims[1] != arg2_requisition.dims[1] &&
       arg1_requisition.dims[1] != out_requisition.dims[1])) {
    g_error ("Incorrect volume size.");
    return NULL;
  }

  cl_mem d_arg1 = ufo_buffer_get_device_image (arg1, command_queue);
  cl_mem d_arg2 = ufo_buffer_get_device_image (arg2, command_queue);
  cl_mem d_out = ufo_buffer_get_device_image (out, command_queue);
  cl_kernel kernel = ufo_resources_get_kernel (resources, OPS_FILENAME, kernel_name, &error);
  if (error) {
    g_error (error->message);
    return NULL;
  }

  UFO_CHECK_CLERR (clSetKernelArg(kernel, 0, sizeof(void *), (void *) &d_arg1));
  UFO_CHECK_CLERR (clSetKernelArg(kernel, 1, sizeof(void *), (void *) &d_arg2));
  UFO_CHECK_CLERR (clSetKernelArg(kernel, 2, sizeof(void *), (void *) &d_out));

  UFO_CHECK_CLERR (clEnqueueNDRangeKernel(command_queue,
                                          kernel,
                                          arg1_requisition.n_dims,
                                          NULL,
                                          arg1_requisition.dims,
                                          NULL,  
                                          0, NULL, &event));
  return event;
}

cl_event
operation2 (const gchar *kernel_name, 
            UfoBuffer *arg1,
            UfoBuffer *arg2,
            gfloat modifier,
            UfoBuffer *out,
            UfoResources *resources,
            cl_command_queue command_queue)
{
  cl_event event;
  GError *error = NULL;
  UfoRequisition arg1_requisition, arg2_requisition, out_requisition;
  ufo_buffer_get_requisition (arg1, &arg1_requisition);
  ufo_buffer_get_requisition (arg2, &arg2_requisition);
  ufo_buffer_get_requisition (out, &out_requisition);

  if ((arg1_requisition.dims[0] != arg2_requisition.dims[0] && 
       arg1_requisition.dims[0] != out_requisition.dims[0]) ||
      (arg1_requisition.dims[1] != arg2_requisition.dims[1] &&
       arg1_requisition.dims[1] != out_requisition.dims[1])) {
    g_error ("Incorrect volume size.");
    return NULL;
  }

  cl_mem d_arg1 = ufo_buffer_get_device_image (arg1, command_queue);
  cl_mem d_arg2 = ufo_buffer_get_device_image (arg2, command_queue);
  cl_mem d_out = ufo_buffer_get_device_image (out, command_queue);
  cl_kernel kernel = ufo_resources_get_kernel (resources, OPS_FILENAME, kernel_name, &error);
  if (error) {
    g_error (error->message);
    return NULL;
  }

  UFO_CHECK_CLERR (clSetKernelArg(kernel, 0, sizeof(void *), (void *) &d_arg1));
  UFO_CHECK_CLERR (clSetKernelArg(kernel, 1, sizeof(void *), (void *) &d_arg2));
  UFO_CHECK_CLERR (clSetKernelArg(kernel, 2, sizeof(gfloat), (void *) &modifier));
  UFO_CHECK_CLERR (clSetKernelArg(kernel, 3, sizeof(void *), (void *) &d_out));

  UFO_CHECK_CLERR (clEnqueueNDRangeKernel(command_queue,
                                          kernel,
                                          arg1_requisition.n_dims,
                                          NULL,
                                          arg1_requisition.dims,
                                          NULL,  
                                          0, NULL, &event));
  return event;
}



cl_event
op_gradient_magnitudes (UfoBuffer *arg,
                        UfoBuffer *out,
                        UfoResources *resources,
                        cl_command_queue command_queue)
{ 
  cl_event event;
  GError *error = NULL;

  UfoRequisition arg_requisition;
  ufo_buffer_get_requisition (arg, &arg_requisition);
  ufo_buffer_resize (out, &arg_requisition);

  cl_mem d_arg = ufo_buffer_get_device_image (arg, command_queue);
  cl_mem d_out = ufo_buffer_get_device_image (out, command_queue);

  cl_kernel kernel = ufo_resources_get_kernel (resources, OPS_FILENAME, "operation_gradient_magnitude", &error);
  if (error) {
    g_error (error->message);
  }

  UFO_CHECK_CLERR (clSetKernelArg(kernel, 0, sizeof(void *), (void *) &d_arg));
  UFO_CHECK_CLERR (clSetKernelArg(kernel, 1, sizeof(void *), (void *) &d_out));

  UFO_CHECK_CLERR (clEnqueueNDRangeKernel(command_queue,
                                          kernel,
                                          arg_requisition.n_dims,
                                          NULL,
                                          arg_requisition.dims,
                                          NULL,  
                                          0, NULL, &event));

  return event;
}

cl_event
op_gradient_directions (UfoBuffer *arg,
                        UfoBuffer *magnitudes, 
                        UfoBuffer *out,
                        UfoResources *resources,
                        cl_command_queue command_queue)
{ 
  cl_event event;
  GError *error = NULL;

  UfoRequisition arg_requisition;
  ufo_buffer_get_requisition (arg, &arg_requisition);
  ufo_buffer_resize (out, &arg_requisition);

  cl_mem d_arg = ufo_buffer_get_device_image (arg, command_queue);
  cl_mem d_magnitudes = ufo_buffer_get_device_image (magnitudes, command_queue);
  cl_mem d_out = ufo_buffer_get_device_image (out, command_queue);

  cl_kernel kernel = ufo_resources_get_kernel (resources, OPS_FILENAME, "operation_gradient_direction", &error);
  if (error) {
    g_error (error->message);
  }

  UFO_CHECK_CLERR (clSetKernelArg(kernel, 0, sizeof(void *), (void *) &d_arg));
  UFO_CHECK_CLERR (clSetKernelArg(kernel, 1, sizeof(void *), (void *) &d_magnitudes));
  UFO_CHECK_CLERR (clSetKernelArg(kernel, 2, sizeof(void *), (void *) &d_out));

  UFO_CHECK_CLERR (clEnqueueNDRangeKernel(command_queue,
                                          kernel,
                                          arg_requisition.n_dims,
                                          NULL,
                                          arg_requisition.dims,
                                          NULL,  
                                          0, NULL, &event));

  return event;
}

gfloat
l1_norma (UfoBuffer *arg,
          UfoResources *resources,
          cl_command_queue command_queue)
{ 
  UfoRequisition arg_requisition;
  ufo_buffer_get_requisition (arg, &arg_requisition);

  gfloat norm = 0;
  gfloat *values = ufo_buffer_get_host_array (arg, command_queue);
  guint i, j;
  for (i = 0; i < arg_requisition.dims[0]; ++i) {
    for (j = 0; j < arg_requisition.dims[1]; ++j) {
      norm += fabs(values[i * arg_requisition.dims[0] + j]);
    }
  }

 return norm;
}

gfloat
euclidean_distance (UfoBuffer *arg1,
                    UfoBuffer *arg2,
                    UfoResources *resources,
                    cl_command_queue command_queue)
{
  UfoRequisition arg1_requisition, arg2_requisition;
  ufo_buffer_get_requisition (arg1, &arg1_requisition);
  ufo_buffer_get_requisition (arg2, &arg2_requisition);
  guint length1 = 0, length2 = 0, length;

  guint i;
  for (i = 0; i < arg1_requisition.n_dims; ++i) {
    length1 += arg1_requisition.dims[i];
  }
  for (i = 0; i < arg1_requisition.n_dims; ++i) {
    length2 += arg2_requisition.dims[i];
  }

  if (length2 != length1) {
    g_warning ("Sizes of buffers are not the same. Zero-padding applied.");
  }
  length = length2 < length1 ? length2 : length1;
  gfloat norm = 0;
  gfloat diff;
  gfloat *values1 = ufo_buffer_get_host_array (arg1, command_queue);
  gfloat *values2 = ufo_buffer_get_host_array (arg2, command_queue);
  
  for (i = 0; i < length; ++i) {
      diff = values1[i] - values2[i];
      norm += pow (diff, 2);
  }
  for (i = length; i < length2; ++i) {
      norm += pow (values2[i], 2);
  }
  for (i = length; i < length1; ++i) {
      norm += pow (values1[i], 2);
  }
  norm = sqrt(norm);
  return norm;
}

gfloat
l2_norma (UfoBuffer *arg,
           UfoResources *resources,
           cl_command_queue command_queue)
{
  return euclidean_distance (arg, arg, resources, command_queue);
}

cl_event
POSC (UfoBuffer *arg,
      UfoBuffer *out,
      UfoResources *resources,
      cl_command_queue command_queue)
{
  cl_event event;
  GError *error = NULL;

  UfoRequisition arg_requisition;
  ufo_buffer_get_requisition (arg, &arg_requisition);
  ufo_buffer_resize (out, &arg_requisition);

  cl_mem d_arg = ufo_buffer_get_device_image (arg, command_queue);
  cl_mem d_out = ufo_buffer_get_device_image (out, command_queue);

  cl_kernel kernel = ufo_resources_get_kernel (resources, OPS_FILENAME, "POSC", &error);
  if (error) {
    g_error (error->message);
  }

  UFO_CHECK_CLERR (clSetKernelArg(kernel, 0, sizeof(void *), (void *) &d_arg));
  UFO_CHECK_CLERR (clSetKernelArg(kernel, 1, sizeof(void *), (void *) &d_out));

  UFO_CHECK_CLERR (clEnqueueNDRangeKernel(command_queue,
                                          kernel,
                                          arg_requisition.n_dims,
                                          NULL,
                                          arg_requisition.dims,
                                          NULL,  
                                          0, NULL, &event));
  return event;
}

cl_event
gradient_descent (UfoBuffer *arg,
                  UfoBuffer *out,
                  UfoResources *resources,
                  cl_command_queue command_queue)
{
  cl_event event;
  GError *error = NULL;

  UfoRequisition arg_requisition;
  ufo_buffer_get_requisition (arg, &arg_requisition);
  ufo_buffer_resize (out, &arg_requisition);

  cl_mem d_arg = ufo_buffer_get_device_image (arg, command_queue);
  cl_mem d_out = ufo_buffer_get_device_image (out, command_queue);

  cl_kernel kernel = ufo_resources_get_kernel (resources, OPS_FILENAME, "descent_grad", &error);
  if (error) {
    g_error (error->message);
  }

  UFO_CHECK_CLERR (clSetKernelArg(kernel, 0, sizeof(void *), (void *) &d_arg));
  UFO_CHECK_CLERR (clSetKernelArg(kernel, 1, sizeof(void *), (void *) &d_out));

  UFO_CHECK_CLERR (clEnqueueNDRangeKernel(command_queue,
                                          kernel,
                                          arg_requisition.n_dims,
                                          NULL,
                                          arg_requisition.dims,
                                          NULL,  
                                          0, NULL, &event));
  return event;
}
