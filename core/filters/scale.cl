__kernel void scale (const float factor, __global float *image)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int width = get_global_size(0);
    const int index = idy * width + idx;
    float x = image[index] * factor;
    image[index] = x;
}

