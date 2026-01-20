/*
 * Copyright (C) 2015-2016 Karlsruhe Institute of Technology
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

#ifndef UFO_FFT_H
#define UFO_FFT_H

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <ufo/ufo.h>

typedef struct {
    enum {
        UFO_FFT_1D = 1,
        UFO_FFT_2D,
        UFO_FFT_3D
    } dimensions;

    gsize size[3];
    gsize batch;
    gboolean zeropad;
} UfoFftParameter;

typedef enum {
    UFO_FFT_FORWARD,
    UFO_FFT_BACKWARD
} UfoFftDirection;

typedef struct _UfoFft UfoFft;

UfoFft *ufo_fft_new     (void);
cl_int  ufo_fft_update  (UfoFft            *fft,
                         cl_context         context,
                         cl_command_queue   queue,
                         UfoFftParameter   *param);
cl_int  ufo_fft_execute (UfoFft            *fft,
                         cl_command_queue   queue,
                         UfoProfiler       *profiler,
                         cl_mem             in_mem,
                         cl_mem             out_mem,
                         UfoFftDirection    direction,
                         cl_uint            num_events,
                         cl_event          *event_list,
                         cl_event          *event);
void    ufo_fft_destroy (UfoFft            *fft);

#endif
