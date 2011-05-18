__kernel void fft_spread(__global float *out, __global float *in, const int num_bins)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int dpitch = get_global_size(0)*2;

    /* May diverge but not possible to reduce latency, because num_bins can 
       be arbitrary and not be aligned. */
    if (idx >= num_bins) {
        out[idy*dpitch + idx*2] = 0.0;
        out[idy*dpitch + idx*2 + 1] = 0.0;
    }
    else {
        out[idy*dpitch + idx*2] = in[idy*num_bins + idx];
        out[idy*dpitch + idx*2 + 1] = 0.0;
    }
}

__kernel void fft_pack(__global float *in, __global float *out, const int num_bins)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int dpitch = get_global_size(0)*2;

    if (idx < num_bins) {
        out[idy*num_bins + idx] = in[idy*dpitch + idx*2];
    }
}

