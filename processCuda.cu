/*
 * Authors  : Lincoln Craddock, John Hershey
 * Date     : 2026-04-28
 * Professor: Dr. Gary Zoppetti
 * Class    : CMSC 476 Parallel Programming
 * Description: Uses CUDA to update a 2-dimensional grid of data points
 * updating positions to simulate gravity
 */
#include <cooperative_groups.h>
#include <cooperative_groups/reduce.h>
#include <cuda.h>
#include <thrust/universal_vector.h>

#include "PowderGame.h"

const unsigned THREADS_PER_BLOCK = 256;
const unsigned THREADS_PER_WARP = 32;
//const unsigned WARPS_PER_BLOCK = 8;
namespace cg = cooperative_groups;
thrust::universal_vector<Data> world;

__global__//
void
ProcessPowderCudaGPU (Data* const a, Data* result, unsigned h, unsigned w);

//pass in the input and output grids, and cast to universal_vector implicitly
std::span<Data>
process_powder(){
  
  thrust::universal_vector<Data> result (H * W, { EMPTY, 0 });
  const unsigned NUM_BLOCKS =
    (H * W + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;
  ProcessPowderCudaGPU<<<NUM_BLOCKS, THREADS_PER_BLOCK>>> (world.data().get(), result.data().get(), H, W);
  cudaDeviceSynchronize ();
  return std::span(result.data, H * W);
}

void
set_up_processing ()
{
  world (H * W, {EMPTY, 0});
}

__global__//
void
ProcessPowderCudaGPU (Data* const input, Data* result, unsigned h, unsigned w)
{
  
  cg::thread_block block = cg::this_thread_block ();
  cg::thread_block_tile<THREADS_PER_WARP> warp = 
  cg::tiled_partition<THREADS_PER_WARP> (block);
  unsigned idx =
    block.group_index ().x * block.group_dim ().x + block.thread_index ().x;
  unsigned stride = block.group_dim ().x * gridDim.x;
  for (unsigned i = idx; i < h * w; i += stride){
    Data val = input[i];
    unsigned y = i % h;
    switch (val.type)
    {
      case DIRT:
      {
        if (y > 0 && input[i-1].type == EMPTY)
          result[i - 1] = {DIRT, 0};
        else
          result[i] = {DIRT, 0};
        break;
      }
      case STONE:
      {
        uint32_t dy = 1;
        while (dy <= val.dy + 1 && y >= dy &&
               input[i- dy].type == EMPTY)
          ++dy;
        --dy;
        result[i - dy] = {STONE, dy > 0 ? dy : 0};
        break;
      }
    }
  }
}
