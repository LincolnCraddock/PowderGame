
#include <cooperative_groups.h>
#include <cooperative_groups/reduce.h>
#include <cuda.h>
#include <thrust/universal_vector.h>

#include "PowderGame.h"

#define CUDA
#endif
const unsigned THREADS_PER_BLOCK = 256;
const unsigned THREADS_PER_WARP = 32;
const unsigned WARPS_PER_BLOCK = 8;
namespace cg = cooperative_groups;

__global__//
void
ProcessPowderCudaGPU (Data* const a, Data* result, unsigned h, unsigned w);

//pass in the input and output grids, and the size of the grid
__host__//
void
ProcessPowderCuda(thrust::universal_vector<thrust::universal_vector<Data>> vec, thrust::universal_vector<thrust::universal_vector<Data>> result, unsigned H, unsigned W){

    
    const unsigned NUM_BLOCKS =
    (N + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;
    ProcessPowderCudaGPU<<<NUM_BLOCKS, THREADS_PER_BLOCK>>> (
    vec[0].data ().get (), result[0].data ().get (), N);
    cudaDeviceSynchronize ();
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
    y = idx/w
    switch (val.type)
      {
        case DIRT:
        {
          if (y > 0 && next.type == EMPTY)
          {
            result[i - w] = {DIRT, 0};
          }
          else
          {
            result[i] = {DIRT, 0};
          }
          break;
        }
        case STONE:
        {
          uint32_t dy = 1;
          while (dy <= val.dy + 1 && y >= dy &&
                 input[i- w * dy].type == EMPTY)
            ++dy;
          --dy;
          result[i - w * dy] = {Stone, dy > 0 ? dy : 0}
          break;
        }
      }
  }
  
}