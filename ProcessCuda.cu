
#include <cooperative_groups.h>
#include <cooperative_groups/reduce.h>
#include <cuda.h>
#include <thrust/universal_vector.h>

#include "PowderGame.h"
const unsigned THREADS_PER_BLOCK = 256;
const unsigned THREADS_PER_WARP = 32;
const unsigned WARPS_PER_BLOCK = 8;
namespace cg = cooperative_groups;

__global__//
void
ProcessPowderCudaGPU (Data* const a, Data* result, unsigned n);

//pass in the input and output grids, and the size of the grid
__host__//
void
ProcessPowderCuda(thrust::universal_vector<thrust::universal_vector<Data>> vec, thrust::universal_vector<thrust::universal_vector<Data>> result, unsigned N){

    
    const unsigned NUM_BLOCKS =
    (N + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;
    ProcessPowderCudaGPU<<<NUM_BLOCKS, THREADS_PER_BLOCK>>> (
    vec[0].data ().get (), result[0].data ().get (), N);
    cudaDeviceSynchronize ();
}

__global__//
void
ProcessPowderCudaGPU (Data* const input, Data* result, unsigned n)
{
  
  cg::thread_block block = cg::this_thread_block ();
  cg::thread_block_tile<THREADS_PER_WARP> warp =
  cg::tiled_partition<THREADS_PER_WARP> (block);
  unsigned idx =
    block.group_index ().x * THREADS_PER_BLOCK + block.thread_index ().x;
  unsigned stride = block.group_dim ().x * gridDim.x;
  for (unsigned i = idx; i < n; i += stride){
    Data val = input[i];
    Data next = input[i];//plus something
    result[i] = val;
  }
  
}