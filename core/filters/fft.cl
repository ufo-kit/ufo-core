__kernel void fft_spread(__global float *out, __global float *in, const int width, const int height)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int dpitch = get_global_size(0)*2;

    /* May diverge but not possible to reduce latency, because num_bins can 
       be arbitrary and not be aligned. */
    if ((idy >= height) || (idx >= width)) {
        out[idy*dpitch + idx*2] = 0.0;
        out[idy*dpitch + idx*2 + 1] = 0.0;
    }
    else {
        out[idy*dpitch + idx*2] = in[idy*width + idx];
        out[idy*dpitch + idx*2 + 1] = 0.0;
    }
}

__kernel void fft_pack(__global float *in, __global float *out, const int width)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int dpitch = get_global_size(0)*2;

    if (idx < width)
        out[idy*width + idx] = in[idy*dpitch + 2*idx];
}

__kernel void fft_normalize(__global float *data)
{
    const int idx = get_global_id(0);
    const int dim_fft = get_global_size(0);
    data[2*idx] = data[2*idx] / dim_fft;
}

