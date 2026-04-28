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

__global__//
void
ProcessPowderCudaGPU (Data* const a, Data* result, unsigned h, unsigned w);

//pass in the input and output grids, and the size of the grid
void
ProcessPowderCuda(thrust::universal_vector<std::vector<Data>> vec, thrust::universal_vector<std::vector<Data>> result, unsigned H, unsigned W){
  //thrust::universal_vector<float> result (1);  
  
  const unsigned NUM_BLOCKS =
    (H * W + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;
  ProcessPowderCudaGPU<<<NUM_BLOCKS, THREADS_PER_BLOCK>>> (&vec[0].front(), &result[0].front(), H, W);
  cudaDeviceSynchronize ();
}

//#define process_powder(world,  newWorld, H, W) ProcessPowderCuda(world, newWorld, H, W);
//wrap the function call
std::vector<std::vector<Data>>
process_powder (std::vector<std::vector<Data>>& world, unsigned H, unsigned W)
{ 
  std::vector<std::vector<Data>> newWorld (H, std::vector<Data> (W, { EMPTY, 0 }));
  //thrust::universal_vector<Data> result (1);
  //std::vector<std::vector<Data>> temp23 (world.begin(), world.end());
  //thrust::universal_vector<std::vector<Data>> tests (world.begin(), world.end());
  //thrust::universal_vector<thrust::universal_vector<Data>> test2 = newWorld;    
  ProcessPowderCuda(world,newWorld, H, W);
  return newWorld;
}

__global__//
void
ProcessPowderCudaGPU (Data* const input, Data* result, unsigned h, unsigned w)
{
  
  cg::thread_block block = cg::this_thread_block ();
  cg::thread_block_tile<THREADS_PER_WARP> warp = 
  cg::tiled_partition<THREADS_PER_WARP> (block);
  unsigned idx =
    block.group_index ().x * THREADS_PER_BLOCK + block.thread_index ().x;
  unsigned stride = block.group_dim ().x * gridDim.x;
  for (unsigned i = idx; i < h * w; i += stride){
    Data val = input[i];
    Data next = input[i-w];//plus something
    unsigned y = idx/w;
    switch (val.type)
    {
      case DIRT:
      {
        if (y > 0 && next.type == EMPTY)
          result[i - w] = {DIRT, 0};
        else
          result[i] = {DIRT, 0};
        break;
      }
      case STONE:
      {
        uint32_t dy = 1;
        while (dy <= val.dy + 1 && y >= dy &&
               input[i- w * dy].type == EMPTY)
          ++dy;
        --dy;
        result[i - w * dy] = {STONE, dy > 0 ? dy : 0};
        break;
      }
    }
  }
}