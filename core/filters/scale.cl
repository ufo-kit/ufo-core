__kernel void scale (const float factor, __global float *image)
{
    const int idx = get_global_id(0);
    image[idx] *= factor;
}

