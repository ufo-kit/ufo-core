__constant float mask[9] = { 0.125, 0.125, 0.125, 0.125, 0.5, 0.125, 0.125, 0.125, 0.125 };

__kernel void convolution(__global float *input, __global float *output, __local float *scratch)
{
    size_t idx = get_global_id(0);
    size_t idy = get_global_id(1);
    size_t width = get_global_size(0);
    size_t height = get_global_size(1);
    size_t index = idy*width + idx;

    scratch[get_local_id(1)*get_local_size(0) + get_local_id(0)] = input[index];
    barrier(CLK_LOCAL_MEM_FENCE);

    /* FIXME: Sum up stuff from shared memory */

    int xbegin = idx >= 1 ? 0 : 2 - idx;
    int xend = idx < width-1 ? 2 : width - idx;
    int ybegin = idy >= 1 ? 0 : 2 - idy;
    int yend = idy < width-1 ? 2 : height - idy;

    float sum = 0.0f;
    for (int i = xbegin; i < xend; i++) 
        for (int j = ybegin; j < yend; j++)
            sum += mask[j*3+i] * input[(idy + j - 1)*width + idx + i - 1];
    
    output[index] = sum;
}

__kernel void binarize_inplace(__global float *data, __local float *scratch)
{
    size_t index = get_global_id(1)*get_global_size(0) + get_global_id(0);
    float value = data[index];
    data[index] = value > 0.6f ? 1.0 : 0.0;
}

__kernel void mirror(__global float *input, __global float *output, __local float *scratch)
{
    int idx = get_global_id(0);
    int idy = get_global_id(1);
    int width = get_global_size(0);
    int height = get_global_size(1);

    output[(idy * width) + idx] = input[((height-idy+1) * width) + (width - idx)];
}

__kernel void fftshift(__global float *input, __global float *output, __local float *scratch)
{
    int idx = get_global_id(0);
    int idy = get_global_id(1);
    int width = get_global_size(0);
    int height = get_global_size(1);
    int src_index = (idy + height / 2) * width + idx + width / 2;
    
    if ((idx >= width / 2) && (idy < height / 2))
        src_index = (idy + height / 2) * width + idx - width / 2;
    else if ((idx < width / 2) && (idy >= height / 2))
        src_index = (idy - height / 2) * width + idx + width / 2;
    else if ((idx >= width / 2) && (idy >= height / 2))
        src_index = (idy - height / 2) * width + idx - width / 2;
            
    output[(idy * width) + idx] = input[src_index];
}

__kernel void mandelbrot(__global float *data, __local float *scratch)
{
    const int idx = get_global_id(0);
    const int idy = get_global_id(1);
    const int width = get_global_size(0);
    const int height = get_global_size(1);
    const size_t index = idy*width + idx;

    const int max_iterations = 200;

    float re = (float) (idx-width/2) * 0.001f;
    float im = (float) (idy-height/2) * 0.001f; 

    float x = 0.0f, y = 0.0f, xt, yt;
    int i = 0;

    for (; i < max_iterations && x*x+y*y <= 4.0f; i++) {
        xt = x*x - y*y + re;
        yt = 2.0f*x*y + im;
        x = xt;
        y = yt;
    }
    data[index] = 1.0f - ((float) i / max_iterations);
}


