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

kernel void
interpolate (global float *a,
             global float *b,
             global float *output,
             float alpha)
{
    const int index = get_global_id(1) * get_global_size(0) + get_global_id(0);
    output[index] = (1.0f - alpha) * a[index] + alpha * b[index];
}

/*
 * Interpolate two arrays along the horizontal direction, *offset* is a linear
 * offset to the first and the output arrays. In stitching, *weight* is used to
 * match the mean of the second buffer's overlapping region to the first one's.
 */
kernel void
interpolate_horizontally (global float *a,
                          global float *b,
                          global float *output,
                          const int width,
                          const int left_width,
                          const int right_width,
                          const int offset,
                          const float weight)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const float alpha = ((float) idx) / (get_global_size (0) - 1.0f);

    output[idy * width + idx + offset] = (1.0f - alpha) * a[idy * left_width + idx + offset] +
                                         alpha * weight * b[idy * right_width + idx];
}

kernel void
interpolate_mask_horizontally (global float *input,
                               global float *mask,
                               global float *output,
                               const int use_gradient)
{
   const int idx = get_global_id (0);
   const int idy = get_global_id (1);
   const int width = get_global_size (0);
   const int offset = idy * width;
   int left = idx, right = idx;
   float span, diff;

   if (mask[offset + idx]) {
        while (left > -1 && mask[offset + left] > 0) {
            left--;
        }
        while (right < width && mask[offset + right] > 0) {
            right++;
        }

        if (left < 0) {
            /* Mask spans to the left border, use only the right value */
            if (right < width - 1) {
                if (use_gradient) {
                    /* There are two valid pixels on the right, use gradient */
                    diff = input[offset + right] - input[offset + right + 1];
                    output[offset + idx] = input[offset + right] + diff * (right - idx);
                } else {
                    output[offset + idx] = input[offset + right];
                }
            } else if (right == width - 1) {
                /* There is only one valid pixel on the right, which is the only
                 * valid pixel in this row, so use it's value everywhere */
                output[offset + idx] = input[offset + right];
            } else {
                /* All pixels in this row are invalid, just copy the input */
                output[offset + idx] = input[offset + idx];
            }
        } else if (right >= width) {
            /* Mask spans to the right border, use only the left value */
            if (left > 0) {
                if (use_gradient) {
                    /* There are two valid pixels on the left, use gradient */
                    diff = input[offset + left] - input[offset + left - 1];
                    output[offset + idx] = input[offset + left] + diff * (idx - left);
                } else {
                    output[offset + idx] = input[offset + left];
                }
            } else if (left == 0) {
                /* There is only one valid pixel on the left, which is the only
                 * valid pixel in this row, so use it's value everywhere */
                output[offset + idx] = input[offset + left];
            }
        } else {
            /* There are valid pixels on both sides, use standard linear
             * interpolation */
            span = (float) (right - left);
            output[offset + idx] = (right - idx) / span * input[offset + left] +
                                   (idx - left) / span * input[offset + right];
        }
   } else {
       output[offset + idx] = input[offset + idx];
   }
}
