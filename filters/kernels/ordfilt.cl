/*
 * Copyright (C) 2013-2016 Karlsruhe Institute of Technology
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

#include "piv.cl"


/* For each pixel, sorts neighboring elements, picks the low_threshold and
 * high_threshold pixel, computes a value, and stores it in the dst image */
kernel void
bitonic_ordfilt (global const float *src, global float *dst, int num_elements,
                 int arrayLength, float low_threshold, float high_threshold,
                 local float *s_key, unsigned idx_offset)
{
    // ascendent order
    unsigned dir = 1;
    unsigned block_idx = get_global_id(0) / get_local_size(0);
    //Offset to the beginning of subbatch and load data
    global const float *d_SrcKey = src + block_idx * num_elements;

    s_key[get_local_id(0)] = d_SrcKey[get_local_id(0)];
    unsigned next_index = get_local_id(0) + arrayLength / 2;

    s_key[next_index] = next_index < num_elements ? d_SrcKey[next_index] : 0xFFFF;

    for (int size = 2; size < arrayLength; size <<= 1) {
        //Bitonic merge.  ddd tells if ordering is ascendent or descendent
        unsigned ddd = dir ^ ((get_local_id(0) & (size / 2)) != 0);

        for (int stride = size / 2; stride > 0; stride >>= 1) {
            barrier(CLK_LOCAL_MEM_FENCE);
            unsigned pos = 2 * get_local_id(0) - (get_local_id(0) & (stride - 1));
            compare_and_exchange (&s_key[pos], &s_key[pos + stride], ddd);
        }
    }

    {
        for (int stride = arrayLength / 2; stride > 0; stride >>= 1) {
            barrier(CLK_LOCAL_MEM_FENCE);
            unsigned pos = 2 * get_local_id(0) - (get_local_id(0) & (stride - 1));
            compare_and_exchange (&s_key[pos], &s_key[pos + stride], dir);
        }
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    if (get_local_id(0) == 0) {
        float low_p = s_key[(int) (num_elements * low_threshold - 1)];
        float high_p = s_key[(int) (num_elements * high_threshold - 1)];

        unsigned global_idx = block_idx + idx_offset;
        float intensity = (high_p + low_p) / 2.;
        float contrast = high_p - low_p;
        dst[global_idx] = intensity * (1 - contrast);
    }
}

kernel void
/* Load only elements that match the pattern */
/* Dst is the size of image * number of ones found in pattern */
load_elements_from_pattern (global float *src,
                            global float *dst,
                            global float *pattern,
                            int dimension,
                            int num_ones,
                            unsigned height,
                            unsigned y_offset)
{
    const int2 imsize = (int2) (get_global_size(0), height);
    const int x = get_global_id(0);
    const int y = get_global_id(1) + y_offset;
    const int local_y = get_global_id(1);

    unsigned short buffer[12];

    int2 center;
    get_center (dimension, dimension, &center);
    /* Don't apply the offset for the dst buffer */
    int offset =  num_ones * (x + local_y * imsize.x);
    global float* tmpDst = dst + offset;
    for(int j = 0; j < dimension; ++j) {
        for(int i = 0; i < dimension; ++i) {
            if (pattern[i + j * dimension]) {
                *tmpDst = src[get_pel_position (imsize, (int2)(i,j), (int2)(x, y), center)];
                ++tmpDst;
            }
        }
    }
}
