/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

void block_fn(size_t tid, int mul, __global int* res)
{
  res[tid] = mul * 7 - 21;
}

kernel void simple_block_kernel(__global int* res)
{
  int multiplier = 3;
  size_t tid = get_global_id(0);

  void (^kernelBlock)(void) = ^{ block_fn(tid, multiplier, res); };

  res[tid] = -1;
  queue_t def_q = get_default_queue();
  ndrange_t ndrange = ndrange_1D(1);
  int enq_res = enqueue_kernel(def_q, CLK_ENQUEUE_FLAGS_WAIT_KERNEL, ndrange, kernelBlock);
  if(enq_res != CLK_SUCCESS) { res[tid] = -1; return; }
}

void block_reflection(size_t scalar, __global uint* buffer, read_only image3d_t img, sampler_t sampler)
{
  float4 color;
  int4 coord;
  buffer[0] = scalar;
  coord.x = scalar;
  coord.y = 0;
  coord.z = 0;
  
  int width = get_image_width( img );
  int heigth = get_image_height( img );
  int depth = get_image_depth( img );
  
  int order = get_image_channel_order( img );
  int type = get_image_channel_data_type( img );
  
  color = read_imagef( img, sampler, coord );
  buffer[1] = (uint)color.x;
  buffer[2] = (uint)width;
  buffer[3] = (uint)heigth;
  buffer[4] = (uint)depth;
  buffer[5] = (uint)order;
  buffer[6] = (uint)type;
  
  sampler_t samplerA = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;
  color = read_imagef( img, samplerA, coord );
  
  buffer[7] = (uint)color.y;
  
  queue_t def_q = get_default_queue();
  ndrange_t ndrange = ndrange_1D(1);
  if( scalar > 2 ){
     scalar--;
	 int enq_res = enqueue_kernel(def_q, CLK_ENQUEUE_FLAGS_WAIT_KERNEL, ndrange, ^{block_reflection(scalar, buffer, img, sampler);});
  }
}

kernel void kernel_reflection(sampler_t sampler, read_only image3d_t img, __global uint* buffer, size_t scalar )
{
  size_t tid = get_global_id(0);

  void (^kernelBlock)(void) = ^{ block_reflection(scalar, buffer, img, sampler); };

  queue_t def_q = get_default_queue();
  ndrange_t ndrange = ndrange_1D(1);
  if( scalar > 0 ){
    int enq_res = enqueue_kernel(def_q, CLK_ENQUEUE_FLAGS_WAIT_KERNEL, ndrange, kernelBlock);
    if(enq_res != CLK_SUCCESS) { buffer[2] = 1; }
  }
}
