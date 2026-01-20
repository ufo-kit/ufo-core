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

constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                             CLK_ADDRESS_CLAMP |
                             CLK_FILTER_LINEAR;

kernel void
forwardproject(read_only image2d_t slice,
               global float *sinogram,
               float axis_pos,
               float angle_step)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int slice_width = get_global_size(0);

    const float angle = idy * angle_step;
    /* radius of object circle */
    const float r = fmin (axis_pos, slice_width - axis_pos);
    /* positive/negative distance from detector center */
    const float d = idx - axis_pos + 0.5f;
    /* length of the cut through the circle */
    const float l = sqrt(4.0f*r*r - 4.0f*d*d);

    /* vector in detector direction */
    float2 D = (float2) (cos(angle), sin(angle));
    D = normalize(D);

    /* vector perpendicular to the detector */
    const float2 N = (float2) (D.y, -D.x);

    /* sample point in the circle traveling along N */
    float2 sample = d * D - l/2.0f * N + ((float2) (axis_pos, axis_pos));
    float sum = 0.0f;

    for (int i = 0; i < l; i++) {
        sum += read_imagef(slice, sampler, sample).x;
        sample += N;
    }

    sinogram[idy * slice_width + idx] = sum;
}
