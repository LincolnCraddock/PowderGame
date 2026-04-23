
#include <cooperative_groups.h>
#include <cooperative_groups/reduce.h>
#include <cuda.h>
#include <thrust/universal_vector.h>
const unsigned THREADS_PER_BLOCK = 256;
const unsigned THREADS_PER_WARP = 32;
const unsigned WARPS_PER_BLOCK = 8;
namespace cg = cooperative_groups;

__global__
void
ProcessPowderCudaGPU (float* const a, float* result, unsigned n);

__host__
void
ProcessPowderCuda(thrust::universal_vector vec, thrust::universal_vector result, unsigned N){

    
    const unsigned NUM_BLOCKS =
    (N + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;
    ProcessPowderCudaGPU<<<NUM_BLOCKS, THREADS_PER_BLOCK>>> (
    vec.data ().get (), result.data ().get (), N);
    cudaDeviceSynchronize ();
}

__global__
void
ProcessPowderCudaGPU (float* const a, float* result, unsigned n)
{
  
  cg::thread_block block = cg::this_thread_block ();
  cg::thread_block_tile<THREADS_PER_WARP> warp =
  cg::tiled_partition<THREADS_PER_WARP> (block);

  // each thread multiplies part of the vector
  unsigned idx =
    block.group_index ().x * THREADS_PER_BLOCK + block.thread_index ().x;
  float prod = idx < n ? a[idx] * b[idx] : 0.0f;
  // reduce threads -> warps
  float sum1 = reduce (warp, prod, cg::plus<float> ());
  __shared__ float sum1s[WARPS_PER_BLOCK];
  if (warp.thread_rank () == 0)
  {
    sum1s[warp.meta_group_rank ()] = sum1;
  }
  // reduce warps -> blocks
  sync (block);
  if (block.thread_rank () < WARPS_PER_BLOCK)
  {
    // all 8 threads here are in warp 0
    float sum2 = reduce (
      cg::coalesced_threads (), sum1s[warp.thread_rank ()], cg::plus<float> ());
    if (block.thread_rank () == 0)
    
  }
}