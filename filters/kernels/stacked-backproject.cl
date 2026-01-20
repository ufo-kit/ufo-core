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

constant sampler_t volumeSampler_single = CLK_NORMALIZED_COORDS_FALSE |
                                          CLK_ADDRESS_CLAMP |
                                          CLK_FILTER_LINEAR;

constant sampler_t volumeSampler_half = CLK_NORMALIZED_COORDS_FALSE |
                                          CLK_ADDRESS_CLAMP |
                                          CLK_FILTER_NEAREST;

constant sampler_t volumeSampler_int8 = CLK_NORMALIZED_COORDS_FALSE |
                                      CLK_ADDRESS_CLAMP_TO_EDGE |
                                      CLK_FILTER_NEAREST;

kernel void
interleave_single ( global float *sinogram,
                    write_only image2d_array_t interleaved_sinograms)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int idz = get_global_id(2);
    const int sizex = get_global_size(0);
    const int sizey = get_global_size(1);

    int sinogram_offset = idz*2;

    float x = sinogram[idx + idy * sizex + (sinogram_offset) * sizex * sizey];
    float y = sinogram[idx + idy * sizex + (sinogram_offset+1) * sizex * sizey];

    write_imagef(interleaved_sinograms, (int4)(idx, idy, idz, 0),(float4)(x,y,0.0f,0.0f));
}

kernel void
texture_single (
        read_only image2d_array_t sinogram,
        global float2 *reconstructed_buffer,
        constant float *sin_lut,
        constant float *cos_lut,
        const unsigned int x_offset,
        const unsigned int y_offset,
        const unsigned int angle_offset,
        const unsigned int n_projections,
        const float axis_pos,
        unsigned long size){

        const int local_idx = get_local_id(0);
        const int local_idy = get_local_id(1);

        const int global_idx = get_global_id(0);
        const int global_idy = get_global_id(1);
        const int idz = get_global_id(2);

        int local_sizex = get_local_size(0);
        int local_sizey = get_local_size(1);

        int global_sizex = get_global_size(0);
        int global_sizey = get_global_size(1);

        /* Computing sequential numbers of 4x4 square, quadrant, and pixel within quadrant */
        int square = local_idy%4;
        int quadrant = local_idx/4;
        int pixel = local_idx%4;

        /* Computing projection and pixel offsets */
        int projection_index = local_idy/4;

        int2 remapped_index_local   = {(4*square + 2*(quadrant%2) + (pixel%2)),
                                       (2* (quadrant/2) + (pixel/2))};

        int2 remapped_index_global  = {(get_group_id(0)*get_local_size(0)+remapped_index_local.x),
                                        (get_group_id(1)*get_local_size(1)+remapped_index_local.y)};

        float2 pixel_coord = {(remapped_index_global.x - axis_pos + x_offset + 0.5f),
                              (remapped_index_global.y - axis_pos + y_offset+0.5f)}; //bx and by

        float2 sum[4] = {0.0f,0.0f};
        __local float2 shared_mem[64][4];
        __local float2 reconstructed_cache[16][16];


        for(int proj = projection_index; proj < n_projections; proj+=4) {
            float sine_value = sin_lut[angle_offset + proj];
            float h = pixel_coord.x * cos_lut[angle_offset + proj] - pixel_coord.y * sin_lut[angle_offset + proj] + axis_pos;
            for(int q=0; q<4; q+=1){
                   sum[q] += read_imagef(sinogram, volumeSampler_single, (float4)(h-4*q*sine_value, proj + 0.5f,idz, 0.0)).xy;
            }
        }

        int2 remapped_index = {(local_idx%4), (4*local_idy + (local_idx/4))};

        for(int q=0; q<4;q+=1){
            /* Moving partial sums to shared memory */
            shared_mem[(local_sizex*remapped_index_local.y + remapped_index_local.x)][projection_index] = sum[q];

            barrier(CLK_LOCAL_MEM_FENCE); // syncthreads

            for(int i=2; i>=1; i/=2){
                if(remapped_index.x <i){
                    shared_mem[remapped_index.y][remapped_index.x] += shared_mem[remapped_index.y][remapped_index.x+i];
                }
                barrier(CLK_GLOBAL_MEM_FENCE); // syncthreads
            }

            if(remapped_index.x == 0){
                reconstructed_cache[4*q+remapped_index.y/16][remapped_index.y%16] = shared_mem[remapped_index.y][0];
            }
            barrier(CLK_LOCAL_MEM_FENCE); // syncthreads
        }

        reconstructed_buffer[global_idx + global_idy*size + idz*size*size] = reconstructed_cache[local_idy][local_idx] * M_PI_F / n_projections;
}

kernel void
uninterleave_single (global float2 *reconstructed_buffer,
                global float *output)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int idz = get_global_id(2);
    const int sizex = get_global_size(0);
    const int sizey = get_global_size(1);
    int output_offset = idz*2;

    output[idx + idy*sizex + (output_offset)*sizex*sizey] = reconstructed_buffer[idx + idy*sizex + idz*sizex*sizey].x;
    output[idx + idy*sizex + (output_offset+1)*sizex*sizey] = reconstructed_buffer[idx + idy*sizex + idz*sizex*sizey].y;
}

kernel void
interleave_half (global float *sinogram,
                 write_only image2d_array_t interleaved_sinograms)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int idz = get_global_id(2);
    const int sizex = get_global_size(0);
    const int sizey = get_global_size(1);

    int sinogram_offset = idz*4;

    float4 b = {sinogram[idx + idy * sizex + (sinogram_offset) * sizex * sizey],
                sinogram[idx + idy * sizex + (sinogram_offset+1) * sizex * sizey],
                sinogram[idx + idy * sizex + (sinogram_offset+2) * sizex * sizey],
                sinogram[idx + idy * sizex + (sinogram_offset+3) * sizex * sizey]};

    // At each pixel, pack 4 slices in Z-projection
    write_imagef(interleaved_sinograms, (int4)(idx, idy, idz, 0),(float4)(b));
}

kernel void
texture_half (
        read_only image2d_array_t sinogram,
        global float4 *reconstructed_buffer,
        constant float *sin_lut,
        constant float *cos_lut,
        const unsigned int x_offset,
        const unsigned int y_offset,
        const unsigned int angle_offset,
        const unsigned int n_projections,
        const float axis_pos,
        unsigned long size){

    const int local_idx = get_local_id(0);
    const int local_idy = get_local_id(1);

    const int global_idx = get_global_id(0);
    const int global_idy = get_global_id(1);
    const int idz = get_global_id(2);

    int local_sizex = get_local_size(0);
    int local_sizey = get_local_size(1);

    int global_sizex = get_global_size(0);
    int global_sizey = get_global_size(1);

    /* Computing sequential numbers of 4x4 square, quadrant, and pixel within quadrant */
    int square = local_idy%4;
    int quadrant = local_idx/4;
    int pixel = local_idx%4;

    /* Computing projection and pixel offsets */
    int projection_index = local_idy/4;
    int2 remapped_index_local   = {(4*square + 2*(quadrant%2) + (pixel%2)),(2* (quadrant/2) + (pixel/2))};
    int2 remapped_index_global  = {(get_group_id(0)*get_local_size(0)+remapped_index_local.x),
                                    (get_group_id(1)*get_local_size(1)+remapped_index_local.y)};


    float2 pixel_coord = {(remapped_index_global.x-axis_pos+x_offset+0.5f), (remapped_index_global.y-axis_pos+y_offset+0.5f)}; //bx and by
    float4 sum[4] = {0.0f,0.0f,0.0f,0.0f};
    __local float4 shared_mem[64][4];
    __local float4 reconstructed_cache[16][16];


    for(int proj = projection_index; proj < n_projections; proj+=4) {
        float sine_value = sin_lut[angle_offset + proj];
        float h = pixel_coord.x * cos_lut[angle_offset + proj] - pixel_coord.y * sin_lut[angle_offset + proj] + axis_pos;
        for(int q=0; q<4; q+=1){
           sum[q] += read_imagef(sinogram, volumeSampler_half, (float4)(h-4*q*sine_value, proj + 0.5f,idz, 0.0));
        }
    }

    int2 remapped_index = {(local_idx%4), (4*local_idy + (local_idx/4))};

    for(int q=0; q<4;q+=1){
        /* Moving partial sums to shared memory */
        shared_mem[(local_sizex*remapped_index_local.y + remapped_index_local.x)][projection_index] = sum[q];

        barrier(CLK_LOCAL_MEM_FENCE); // syncthreads

        for(int i=2; i>=1; i/=2){
            if(remapped_index.x <i){
                shared_mem[remapped_index.y][remapped_index.x] += shared_mem[remapped_index.y][remapped_index.x+i];
            }
            barrier(CLK_GLOBAL_MEM_FENCE); // syncthreads
        }

        if(remapped_index.x == 0){
            reconstructed_cache[4*q+remapped_index.y/16][remapped_index.y%16] = shared_mem[remapped_index.y][0];
        }
        barrier(CLK_LOCAL_MEM_FENCE); // syncthreads
    }
    reconstructed_buffer[global_idx + global_idy*size + idz*size*size] = reconstructed_cache[local_idy][local_idx] * M_PI_F / n_projections;
}

kernel void
uninterleave_half (global float4 *reconstructed_buffer,
                   global float *output)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int idz = get_global_id(2);
    const int sizex = get_global_size(0);
    const int sizey = get_global_size(1);

    int output_offset = idz*4;

    float4 b = reconstructed_buffer[idx + idy*sizex + idz*sizex*sizey];

    output[idx + idy*sizex + (output_offset)*sizex*sizey] = b.x;
    output[idx + idy*sizex + (output_offset+1)*sizex*sizey] = b.y;
    output[idx + idy*sizex + (output_offset+2)*sizex*sizey] = b.z;
    output[idx + idy*sizex + (output_offset+3)*sizex*sizey] = b.w;
}

kernel void
interleave_uint (global float *sinogram,
            write_only image2d_array_t interleaved_sinograms,
            const float min,
            const float max)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int idz = get_global_id(2);
    const int sizex = get_global_size(0);
    const int sizey = get_global_size(1);

    int sinogram_offset = idz*4;

    const float scale = 255.0f / (max - min);

    uint4 b = {(sinogram[idx + idy * sizex + (sinogram_offset) * sizex * sizey] - min)*scale,
               (sinogram[idx + idy * sizex + (sinogram_offset+1) * sizex * sizey] - min)*scale,
               (sinogram[idx + idy * sizex + (sinogram_offset+2) * sizex * sizey] - min)*scale,
               (sinogram[idx + idy * sizex + (sinogram_offset+3) * sizex * sizey] - min)*scale};

   write_imageui(interleaved_sinograms, (int4)(idx, idy, idz, 0),(uint4)(b));
}

kernel void
texture_uint (
        read_only image2d_array_t sinogram,
        global uint4 *reconstructed_buffer,
        constant float *sin_lut,
        constant float *cos_lut,
        const unsigned int x_offset,
        const unsigned int y_offset,
        const unsigned int angle_offset,
        const unsigned int n_projections,
        const float axis_pos,
        unsigned long size){

        const int local_idx = get_local_id(0);
        const int local_idy = get_local_id(1);

        const int global_idx = get_global_id(0);
        const int global_idy = get_global_id(1);
        const int idz = get_global_id(2);

        int local_sizex = get_local_size(0);
        int local_sizey = get_local_size(1);

        int global_sizex = get_global_size(0);
        int global_sizey = get_global_size(1);

        /* Computing sequential numbers of 4x4 square, quadrant, and pixel within quadrant */
        int square = local_idy%4;
        int quadrant = local_idx/4;
        int pixel = local_idx%4;

        /* Computing projection and pixel offsets */
        int projection_index = local_idy/4;
        int2 remapped_index_local   = {(4*square + 2*(quadrant%2) + (pixel%2)),(2* (quadrant/2) + (pixel/2))};
        int2 remapped_index_global  = {(get_group_id(0)*get_local_size(0)+remapped_index_local.x),
                                        (get_group_id(1)*get_local_size(1)+remapped_index_local.y)};

        float2 pixel_coord = {(remapped_index_global.x-axis_pos+x_offset+0.5f), (remapped_index_global.y-axis_pos+y_offset+0.5f)}; //bx and by

        uint4 sum[4] = {0,0,0,0};
        __local uint4 shared_mem[64][4];
        __local uint4 reconstructed_cache[16][16];

        for(int proj = projection_index; proj < n_projections; proj+=4) {
            float sine_value = sin_lut[angle_offset + proj];
            float h = pixel_coord.x * cos_lut[angle_offset + proj] - pixel_coord.y * sin_lut[angle_offset + proj] + axis_pos;
            for(int q=0; q<4; q+=1){
               sum[q] += read_imageui(sinogram, volumeSampler_int8, (float4)(h-4*q*sine_value, proj + 0.5f,idz, 0.0));
            }
        }


        int2 remapped_index = {(local_idx%4), (4*local_idy + (local_idx/4))};

        for(int q=0; q<4;q+=1){
            /* Moving partial sums to shared memory */
            shared_mem[(local_sizex*remapped_index_local.y + remapped_index_local.x)][projection_index] = sum[q];

            barrier(CLK_LOCAL_MEM_FENCE); // syncthreads

            for(int i=2; i>=1; i/=2){
                if(remapped_index.x <i){
                    shared_mem[remapped_index.y][remapped_index.x] += shared_mem[remapped_index.y][remapped_index.x+i];
                }
                barrier(CLK_GLOBAL_MEM_FENCE); // syncthreads
            }

            if(remapped_index.x == 0){
                reconstructed_cache[4*q+remapped_index.y/16][remapped_index.y%16] = shared_mem[remapped_index.y][0];
            }
            barrier(CLK_LOCAL_MEM_FENCE); // syncthreads
        }

        reconstructed_buffer[global_idx + global_idy*size + idz*size*size] = reconstructed_cache[local_idy][local_idx];
}

kernel void
uninterleave_uint (global uint4 *reconstructed_buffer,
                global float *output,
                const float min,
                const float max,
                const unsigned int n_projections)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int idz = get_global_id(2);
    const int sizex = get_global_size(0);
    const int sizey = get_global_size(1);

    int output_offset = idz*4;
    float scale = (max-min)/255.0f;

    output[idx + idy*sizex + (output_offset)*sizex*sizey] = ((reconstructed_buffer[idx + idy*sizex + idz*sizex*sizey].x)*scale+min)* M_PI_F / n_projections;
    output[idx + idy*sizex + (output_offset+1)*sizex*sizey] = ((reconstructed_buffer[idx + idy*sizex + idz*sizex*sizey].y)*scale+min)* M_PI_F / n_projections;
    output[idx + idy*sizex + (output_offset+2)*sizex*sizey] = ((reconstructed_buffer[idx + idy*sizex + idz*sizex*sizey].z)*scale+min)* M_PI_F / n_projections;
    output[idx + idy*sizex + (output_offset+3)*sizex*sizey] = ((reconstructed_buffer[idx + idy*sizex + idz*sizex*sizey].w)*scale+min)* M_PI_F / n_projections;
}