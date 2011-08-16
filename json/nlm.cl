
#define SEARCH_RADIUS   10
#define NB_RADIUS       3
#define SIGMA           8

#define flatten(x,y,r,w) ((y-r)*w + (x-r))

/* Compute the distance of two neighbourhood vectors _starting_ from index i
   and j and edge length radius */
float dist(__global float *input, int i, int j, int radius, int image_width)
{
    float dist = 0.0, tmp;
    float wsize = (2.0 * radius + 1.0);
    wsize *= wsize;

    const int nb_width = 2 * radius + 1;
    const int stride = image_width - nb_width;
    for (int k = 0; k < nb_width; k++, i += stride, j += stride) {
        for (int l = 0; l < nb_width; l++, i++, j++) {
            tmp = input[i] - input[j];
            dist += tmp * tmp;
        }
    }
    return dist / wsize;
}

__kernel void nlm(__global float *input, __global float *output, __local float *scratch)
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    const int width = get_global_size(0);
    const int height = get_global_size(1);

    float total_weight = 0.0;
    float pixel_value = 0.0;
    
    /* Compute the upper left (sx,sy) and lower right (tx, ty) corner points
       of our search window */
    int r = min(NB_RADIUS, min(width - 1 - x, min (height - 1 - y, min(x, y))));
    int sx = max(x - SEARCH_RADIUS, r);
    int sy = max(y - SEARCH_RADIUS, r);
    int tx = min(x + SEARCH_RADIUS, width - 1 - r);
    int ty = min(y + SEARCH_RADIUS, height - 1 - r);

    for (int i = sx; i < tx; i++) {
        for (int j = sy; j < ty; j++) {
            float d = dist(input, flatten(x, y, r, width), flatten(i,j,r,width), r, width);
            float weight = exp(- (SIGMA * SIGMA * 100) * d);
            pixel_value += weight * input[j*width + i];
            total_weight += weight;
        }
    }

    output[y*width + x] = pixel_value / total_weight;
}
