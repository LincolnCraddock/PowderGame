/*
 * Authors  : Lincoln Craddock, John Hershey
 * Date     : 2026-04-28
 * Professor: Dr. Gary Zoppetti
 * Class    : CMSC 476 Parallel Programming
 * Description: Description: test file for HIP
  from: https://rocm.docs.amd.com/projects/install-on-windows/en/develop/how-to/debugger-windows.html
  should be compiled with: hipcc example.cpp -o example.exe -gdwarf -Wl,-debug:dwarf -O3
 */
#include <hip/hip_runtime.h>
#include <assert.h>
#include <hip/hip_cooperative_groups.h>
#include <thrust/universal_vector.h>

#include "PowderGame.h"

const unsigned THREADS_PER_BLOCK = 256;
const unsigned THREADS_PER_WARP = 32;
//const unsigned WARPS_PER_BLOCK = 8;
namespace cg = cooperative_groups;

thrust::universal_vector<Data> world;

__global__//
void
ProcessPowderHipGPU (Data* const input, Data* result, unsigned h, unsigned w);

//pass in the input and output grids, and cast to universal_vector implicitly
std::span<Data>
process_powder()
{
  //int *result_ptr, result;
  /* Allocate memory for the device to write the result to. */
  //hipError_t error = hipMalloc(&result_ptr, sizeof(int));
  //assert(error == hipSuccess);
  thrust::universal_vector<Data> result (H * W, { EMPTY, 0 });
  const unsigned NUM_BLOCKS =
    (H * W + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;
  //ProcessPowderCudaGPU<<<NUM_BLOCKS, THREADS_PER_BLOCK>>> (vec.data().get(), result.data().get(), H, W);

  ProcessPowderHipGPU<<<dim3(NUM_BLOCKS), dim3(THREADS_PER_BLOCK), 0, 0>>>(world.data().get(), result.data().get(), H, W);
  
  hipDeviceSynchronize ();
  return std::span<Data> (result.data().get(), H * W);
  /* Copy result from device to host. This acts as a
     synchronization point, waiting for the kernel dispatch to
     complete. */
  // error = hipMemcpyDtoH(&result, result_ptr, sizeof(int));
  // assert(error == hipSuccess);

  // printf("result is %d\n", result);
  // assert(result == 3);
  // return 0;
}

std::span<Data>
set_up_processing ()
{
  world  = thrust::universal_vector<Data>(H * W, {EMPTY, 0});
  return std::span<Data> (world.data().get(), H * W);
}

__global__//
void
ProcessPowderHipGPU (Data* const input, Data* result, unsigned h, unsigned w)
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
      case EMPTY:
        break;
    }
  }
}