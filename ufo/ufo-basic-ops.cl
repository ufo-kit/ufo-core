const sampler_t imageSampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;
const sampler_t imageSampler2 = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

__kernel
void operation_set (__write_only image2d_t out,
                    const float value)
{
  const uint X = get_global_id(0);
  const uint Y = get_global_id(1);

  int2 coord_w;
  coord_w.x = X;
  coord_w.y = Y;

  write_imagef(out, coord_w, value);
}

__kernel
void operation_inv (__read_only image2d_t in,
                 		__write_only image2d_t out)
{
	const uint X = get_global_id(0);
	const uint Y = get_global_id(1);

 	float2 coord_r;
	coord_r.x = (float)X + 0.5f;
	coord_r.y = (float)Y + 0.5f;

	int2 coord_w;
	coord_w.x = X;
	coord_w.y = Y;

	float value = read_imagef(in, imageSampler, coord_r).s0;
  value = (value != 0)? 1.0f / value : 0;
  write_imagef(out, coord_w, value);
}

__kernel
void operation_mul (__read_only image2d_t arg1_r,
                    __read_only image2d_t arg2_r,
                    __write_only image2d_t out)
{
  const uint X = get_global_id(0);
  const uint Y = get_global_id(1);

  float2 coord_r;
  coord_r.x = (float)X + 0.5f;
  coord_r.y = (float)Y + 0.5f;

  int2 coord_w;
  coord_w.x = X;
  coord_w.y = Y;

  float value = read_imagef(arg1_r, imageSampler, coord_r).s0 *
                read_imagef(arg2_r, imageSampler, coord_r).s0;

  write_imagef(out, coord_w, value);
}

__kernel
void operation_add (__read_only image2d_t arg1_r,
                    __read_only image2d_t arg2_r,
                    __write_only image2d_t out)
{
  const uint X = get_global_id(0);
  const uint Y = get_global_id(1);

  float2 coord_r;
  coord_r.x = (float)X + 0.5f;
  coord_r.y = (float)Y + 0.5f;

  int2 coord_w;
  coord_w.x = X;
  coord_w.y = Y;

  float value = read_imagef(arg1_r, imageSampler, coord_r).s0 +
                read_imagef(arg2_r, imageSampler, coord_r).s0;

  write_imagef(out, coord_w, value);
}

__kernel
void operation_deduction (__read_only image2d_t arg1_r,
                          __read_only image2d_t arg2_r,
                          __write_only image2d_t out)
{
  const uint X = get_global_id(0);
  const uint Y = get_global_id(1);

  float2 coord_r;
  coord_r.x = (float)X + 0.5f;
  coord_r.y = (float)Y + 0.5f;

  int2 coord_w;
  coord_w.x = X;
  coord_w.y = Y;

  float value = read_imagef(arg1_r, imageSampler, coord_r).s0 -
                read_imagef(arg2_r, imageSampler, coord_r).s0;

  write_imagef(out, coord_w, value);
}

__kernel
void operation_deduction2 (__read_only image2d_t arg1_r,
                           __read_only image2d_t arg2_r,
                           const float  modifier,
                           __write_only image2d_t out)
{
  const uint X = get_global_id(0);
  const uint Y = get_global_id(1);

  float2 coord_r;
  coord_r.x = (float)X + 0.5f;
  coord_r.y = (float)Y + 0.5f;

  int2 coord_w;
  coord_w.x = X;
  coord_w.y = Y;

  float value = read_imagef(arg1_r, imageSampler, coord_r).s0 -
                modifier * read_imagef(arg2_r, imageSampler, coord_r).s0;

  write_imagef(out, coord_w, value);
}

__kernel
void operation_add2 (__read_only image2d_t arg1_r,
                     __read_only image2d_t arg2_r,
                     const float  modifier,
                     __write_only image2d_t out)
{
  const uint X = get_global_id(0);
  const uint Y = get_global_id(1);

  float2 coord_r;
  coord_r.x = (float)X + 0.5f;
  coord_r.y = (float)Y + 0.5f;

  int2 coord_w;
  coord_w.x = X;
  coord_w.y = Y;

  float value = read_imagef(arg1_r, imageSampler, coord_r).s0 +
                modifier * read_imagef(arg2_r, imageSampler, coord_r).s0;

  write_imagef(out, coord_w, value);
}

__kernel
void op_mulRows (__read_only  image2d_t arg1_r,
                 __read_only  image2d_t arg2_r,
                 __write_only image2d_t out,
                const uint    offset)
{
  const uint X = get_global_id(0);
  const uint Y = get_global_id(1);

  float2 coord_r;
  coord_r.x = (float)X + 0.5f;
  coord_r.y = (float)offset + (float)Y + 0.5f;

  int2 coord_w;
  coord_w.x = X;
  coord_w.y = offset + Y;

  float value = read_imagef(arg1_r, imageSampler, coord_r).s0 *
                read_imagef(arg2_r, imageSampler, coord_r).s0;

  write_imagef(out, coord_w, value);
}

__kernel
void operation_gradient_magnitude (__read_only image2d_t arg_r,
                                   __write_only image2d_t out)
{
  const uint X = get_global_id(0);
  const uint Y = get_global_id(1);

  int2 coord_w;
  coord_w.x = X;
  coord_w.y = Y;

  float2 coord_r[5];
  coord_r[0].x = (float)X + 0.5f;
  coord_r[0].y = (float)Y + 0.5f;
  coord_r[1].x = coord_r[0].x + 1;
  coord_r[1].y = coord_r[0].y;
  coord_r[2].x = coord_r[0].x - 1;
  coord_r[2].y = coord_r[0].y;
  coord_r[3].x = coord_r[0].x;
  coord_r[3].y = coord_r[0].y + 1;
  coord_r[4].x = coord_r[0].x;
  coord_r[4].y = coord_r[0].y - 1;

  float cell_value = read_imagef(arg_r, imageSampler2, coord_r[0]).s0;
  float d1 = read_imagef(arg_r, imageSampler2, coord_r[1]).s0 - cell_value;
  float d2 = read_imagef(arg_r, imageSampler2, coord_r[2]).s0 - cell_value;
  float d3 = read_imagef(arg_r, imageSampler2, coord_r[3]).s0 - cell_value;
  float d4 = read_imagef(arg_r, imageSampler2, coord_r[4]).s0 - cell_value;

  float value = sqrt ( (pow (d1, 2) + pow (d2, 2) + pow (d3, 2) + pow (d4, 2)) / 2.0f);
  write_imagef(out, coord_w, value);
}

__kernel
void operation_gradient_direction (__read_only image2d_t arg_r,
                                   __read_only image2d_t magnitude,
                                   __write_only image2d_t out)
{
  const uint X = get_global_id(0);
  const uint Y = get_global_id(1);

  int2 coord_w;
  coord_w.x = X;
  coord_w.y = Y;

  float2 coord_r[5];
  coord_r[0].x = (float)X + 0.5f;
  coord_r[0].y = (float)Y + 0.5f;
  coord_r[1].x = coord_r[0].x + 1;
  coord_r[1].y = coord_r[0].y;
  coord_r[2].x = coord_r[0].x - 1;
  coord_r[2].y = coord_r[0].y;
  coord_r[3].x = coord_r[0].x;
  coord_r[3].y = coord_r[0].y + 1;
  coord_r[4].x = coord_r[0].x;
  coord_r[4].y = coord_r[0].y - 1;

  float values[5];
  values[0] = read_imagef(arg_r, imageSampler2, coord_r[0]).s0;
  values[1] = read_imagef(arg_r, imageSampler2, coord_r[1]).s0;
  values[2] = read_imagef(arg_r, imageSampler2, coord_r[2]).s0;
  values[3] = read_imagef(arg_r, imageSampler2, coord_r[3]).s0;
  values[4] = read_imagef(arg_r, imageSampler2, coord_r[4]).s0;

  float magnitudes[5];
  magnitudes[0] = read_imagef(magnitude, imageSampler2, coord_r[0]).s0;
  magnitudes[1] = read_imagef(magnitude, imageSampler2, coord_r[1]).s0;
  magnitudes[2] = read_imagef(magnitude, imageSampler2, coord_r[2]).s0;
  magnitudes[3] = read_imagef(magnitude, imageSampler2, coord_r[3]).s0;
  magnitudes[4] = read_imagef(magnitude, imageSampler2, coord_r[4]).s0;

  float direction = 0;
  if (magnitudes[0]) direction += (4 * values[0] - values[1] - values[2] - values[3] - values[4]) / magnitudes[0];
  if (magnitudes[1]) direction += (values[0] - values[1]) / magnitudes[1];
  if (magnitudes[2]) direction += (values[0] - values[2]) / magnitudes[2];
  if (magnitudes[3]) direction += (values[0] - values[3]) / magnitudes[3];
  if (magnitudes[4]) direction += (values[0] - values[4]) / magnitudes[4];

  write_imagef(out, coord_w, direction);
}


__kernel
void POSC (__read_only image2d_t arg_r,
           __write_only image2d_t out)
{
  const uint X = get_global_id(0);
  const uint Y = get_global_id(1);

  int2 coord_w;
  coord_w.x = X;
  coord_w.y = Y;

  float2 coord_r;
  coord_r.x = X + 0.5f;
  coord_r.y = Y + 0.5f;

  float value = read_imagef(arg_r, imageSampler2, coord_r).s0;
  value = value > 0 ? value : 0;
  write_imagef(out, coord_w, value);
}

__kernel
void descent_grad (__read_only image2d_t arg_r,
                   __write_only image2d_t out)
{
  const uint X = get_global_id(0);
  const uint Y = get_global_id(1);

  int2 coord_w;
  coord_w.x = X;
  coord_w.y = Y;

  float2 coord_r[7];
  coord_r[0].x = X;
  coord_r[0].y = Y;
  coord_r[1].x = coord_r[0].x - 1;
  coord_r[1].y = coord_r[0].y;
  coord_r[2].x = coord_r[0].x;
  coord_r[2].y = coord_r[0].y - 1;
  coord_r[3].x = coord_r[0].x + 1;
  coord_r[3].y = coord_r[0].y;
  coord_r[4].x = coord_r[0].x;
  coord_r[4].y = coord_r[0].y + 1;
  coord_r[5].x = coord_r[0].x + 1;
  coord_r[5].y = coord_r[0].y - 1;
  coord_r[6].x = coord_r[0].x - 1;
  coord_r[6].y = coord_r[0].y + 1;

  float eps = 1E-8;
  float values[7];
  values[0] = read_imagef(arg_r, imageSampler2, coord_r[0]).s0;
  values[1] = read_imagef(arg_r, imageSampler2, coord_r[1]).s0;
  values[2] = read_imagef(arg_r, imageSampler2, coord_r[2]).s0;
  values[3] = read_imagef(arg_r, imageSampler2, coord_r[3]).s0;
  values[4] = read_imagef(arg_r, imageSampler2, coord_r[4]).s0;
  values[5] = read_imagef(arg_r, imageSampler2, coord_r[5]).s0;
  values[6] = read_imagef(arg_r, imageSampler2, coord_r[6]).s0;

  float t1, t2;
  float part[3];
  t1 = values[0] - values[1];
  t2 = values[0] - values[2];
  part[0] = (t1 + t2) / sqrt(eps + pow(t1, 2) + pow (t2, 2));
  t1 = values[3] - values[0];
  t2 = values[3] - values[5];
  part[1] = t1 / sqrt(eps + pow(t1, 2) + pow (t2, 2));
  t1 = values[4] - values[0];
  t2 = values[4] - values[6];
  part[2] = t1 / sqrt(eps + pow(t1, 2) + pow (t2, 2));

  float value = part[0] - part[1] - part[2];
  write_imagef(out, coord_w, value);
}