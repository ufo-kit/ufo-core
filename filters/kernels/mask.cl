kernel void
circular (global float *input, global float *output)
{
    const int idx = get_global_id (0);
    const int idy = get_global_id (1);
    const int width = get_global_size(0);
    const int height = get_global_size(1);
    const float x = idx - width / 2;
    const float y = idy - width / 2;

    if (sqrt (x*x + y*y) > min (width / 2, height / 2))
        output[idy * width + idx] = 0.0f;
    else
        output[idy * width + idx] = input[idy * width + idx];
}
