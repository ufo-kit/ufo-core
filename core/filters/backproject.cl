
__kernel void backproject(const int num_proj,
    const int num_bins,
    const float off_x,
    const float off_y,
    __constant float *cos_table,
    __constant float *sin_table,
    __constant float *axis_table,
    __global float *sinogram,
    __global float *slice)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int slice_width = get_global_size(0);

    /* TODO: maybe use float4 for optimal vectorization on Intel CPUs */
    float2 b = (float2) (idx + off_x, idy + off_y);
    float corr;
    float sum = 0.0;

    for(int proj = 0; proj < num_proj; proj++) {
        float2 s = (float2) (cos_table[proj], sin_table[proj]);
        s = s*b;
        corr = axis_table[proj];
        sum += sinogram[(int)(proj*num_bins + corr + s.x - s.y)];
    }
    slice[idy*slice_width + idx] = sum;
}

const sampler_t volumeSampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_LINEAR; 

__kernel void backproject_tex(const int num_proj,
    const int num_bins,
    const float off_x,
    const float off_y,
    __constant float *cos_table,
    __constant float *sin_table,
    __constant float *axis_table,
    __read_only image2d_t sinogram,
    __global float *slice)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int slice_width = get_global_size(0);
    const int slice_index = idy * slice_width + idx;

    /*float x = (idx - slice_width/2);*/
    /*float y = (idy - slice_width/2);*/
    /*x *= x;*/
    /*y *= y;*/
    /*if (sqrt(x + y) > slice_width/2) {*/
        /*slice[slice_index] = 0.0f;*/
        /*return;*/
    /*}*/

    float h;
    const float bx = idx + off_x;
    const float by = -(idy + off_y);
    float sum = 0.0f;

#pragma unroll 8
    for(int proj = 0; proj < num_proj; proj++) {
        h = mad(by, sin_table[proj], mad(bx, cos_table[proj], axis_table[proj]));
        sum += read_imagef(sinogram, volumeSampler, (float2)(h, proj)).x;
    }
    
    slice[slice_index] = sum;
    /*slice[idy*slice_width + idx] = (float) lidx;*/
}

