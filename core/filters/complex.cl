
__kernel void c_add(__global float *in1, __global float *in2, __global float *out)
{
    int idx = get_global_id(1) * 2 * get_global_size(0) + 2 * get_global_id(0);
    
    out[idx] = in1[idx] + in2[idx];
    out[idx+1] = in1[idx+1] + in2[idx+1];
}

__kernel void c_mul(__global float *in1, __global float *in2, __global float *out)
{
    int idx = get_global_id(1) * 2 * get_global_size(0) + 2 * get_global_id(0);
    const float a = in1[idx];
    const float b = in1[idx+1];
    const float c = in2[idx];
    const float d = in2[idx+1];
    
    out[idx] = a*c - b*d;
    out[idx+1] = b*c + a*d;
}

__kernel void c_div(__global float *in1, __global float *in2, __global float *out)
{
    int idx = get_global_id(1) * 2 * get_global_size(0) + 2 * get_global_id(0);
    const float a = in1[idx];
    const float b = in1[idx+1];
    const float c = in2[idx];
    const float d = in2[idx+1];
    float divisor = c*c + d*d;
    
    if (divisor == 0.0f)
        divisor = 0.000000001f;
    
    out[idx] = (a*c + b*d) / divisor;
    out[idx+1] = (b*c - a*d) / divisor;
}

__kernel void c_conj(__global float *data)
{
    int idx = get_global_id(1) * 2 * get_global_size(0) + 2 * get_global_id(0);
    data[idx+1] = -data[idx+1];
}
