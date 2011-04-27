__kernel void scale (const float factor, __global float *image)
{
    int gid = get_global_id(0);
    image[gid] *= factor;
}

