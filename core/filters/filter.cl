__kernel void filter(__global float *data, __global float *filter)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    /* FIXME: we should have a GPU core handle more than just one entry per call */
    const int id = idy*get_global_size(0) + idx;
    data[id] = data[id] * filter[idx];
}
