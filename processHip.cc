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
#include <hip_cooperative_groups.h>
#include <thrust/universal_vector.h>

#include "PowderGame.h"

const unsigned THREADS_PER_BLOCK = 256;
const unsigned THREADS_PER_WARP = 32;
//const unsigned WARPS_PER_BLOCK = 8;
namespace cg = cooperative_groups;

__global__//
void
ProcessPowderHipGPU (Data* const input, Data* result, unsigned h, unsigned w)

__host__//
void
ProcessPowderHip (thrust::universal_vector<thrust::universal_vector<Data>> vec, thrust::universal_vector<thrust::universal_vector<Data>> result, unsigned H, unsigned W)
{
  int *result_ptr, result;
  /* Allocate memory for the device to write the result to. */
  hipError_t error = hipMalloc(&result_ptr, sizeof(int));
  assert(error == hipSuccess);

  /* Run `do_an_addition` on one block containing one HIP thread. */

  ProcessPowderHipGPU<<<dim3(NUM_BLOCKS), dim3(THREADS_PER_BLOCK), 0, 0>>>(vec, result);
  
  /* Copy result from device to host. This acts as a
     synchronization point, waiting for the kernel dispatch to
     complete. */
  error = hipMemcpyDtoH(&result, result_ptr, sizeof(int));
  assert(error == hipSuccess);

  printf("result is %d\n", result);
  assert(result == 3);
  return 0;
}



__global__//
void
ProcessPowderHipGPU (Data* const input, Data* result, unsigned h, unsigned w)
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