/*
 * Copyright (C) 2015-2019 Karlsruhe Institute of Technology
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

#ifndef UFO_COMMON_H
#define UFO_COMMON_H

#include "config.h"
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <ufo/ufo.h>

gfloat          ufo_common_estimate_sigma                  (cl_kernel convolution_kernel,
                                                            cl_kernel sum_kernel,
                                                            cl_command_queue cmd_queue,
                                                            cl_sampler sampler,
                                                            UfoProfiler *profiler,
                                                            cl_mem input_image,
                                                            cl_mem out_mem,
                                                            const gsize max_work_group_size,
                                                            const gsize *global_size);

#endif
