#ifndef __UFO_BASIC_OPS
#define __UFO_BASIC_OPS

#if !defined (__UFO_H_INSIDE__) && !defined (UFO_COMPILATION)
#error "Only <ufo/ufo.h> can be included directly."
#endif

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <glib.h>
#include <ufo/ufo-buffer.h>
#include <ufo/ufo-resources.h>

cl_event
op_set (UfoBuffer *arg, 
        float value,
        UfoResources *resources,
        cl_command_queue command_queue);

cl_event
op_inv (UfoBuffer *arg, 
        UfoResources *resources,
        cl_command_queue command_queue);

cl_event
op_mul (UfoBuffer *arg1,
        UfoBuffer *arg2,
        UfoBuffer *out,
        UfoResources *resources,
        cl_command_queue command_queue);

cl_event
op_add (UfoBuffer *arg1,
        UfoBuffer *arg2,
        UfoBuffer *out,
        UfoResources *resources,
        cl_command_queue command_queue);

cl_event
op_add2 (UfoBuffer *arg1,
         UfoBuffer *arg2,
         gfloat modifier,
         UfoBuffer *out,
         UfoResources *resources,
         cl_command_queue command_queue);

cl_event
op_deduction (UfoBuffer *arg1,
              UfoBuffer *arg2,
              UfoBuffer *out,
              UfoResources *resources,
              cl_command_queue command_queue);

cl_event
op_deduction2 (UfoBuffer *arg1,
               UfoBuffer *arg2,
               gfloat modifier,
               UfoBuffer *out,
               UfoResources *resources,
               cl_command_queue command_queue);

cl_event
op_mulRows (UfoBuffer *arg1,
            UfoBuffer *arg2,
            UfoBuffer *out, 
            guint offset,
            guint n,
            UfoResources *resources,
            cl_command_queue command_queue);

cl_event
op_gradient_magnitudes (UfoBuffer *arg,
                        UfoBuffer *out,
                        UfoResources *resources,
                        cl_command_queue command_queue);

cl_event
op_gradient_directions (UfoBuffer *arg,
                        UfoBuffer *magnitudes, 
                        UfoBuffer *out,
                        UfoResources *resources,
                        cl_command_queue command_queue);


typedef gfloat (*NormaFunct) (UfoBuffer *, UfoResources *, cl_command_queue); 
gfloat
l1_norma (UfoBuffer *arg,
           UfoResources *resources,
           cl_command_queue command_queue);

gfloat
euclidean_distance (UfoBuffer *arg1,
                    UfoBuffer *arg2,
                    UfoResources *resources,
                    cl_command_queue command_queue);

gfloat
l2_norma (UfoBuffer *arg,
           UfoResources *resources,
           cl_command_queue command_queue);

cl_event
POSC (UfoBuffer *arg,
      UfoBuffer *out,
      UfoResources *resources,
      cl_command_queue command_queue);

cl_event
gradient_descent (UfoBuffer *arg,
                  UfoBuffer *out,
                  UfoResources *resources,
                  cl_command_queue command_queue);


#endif