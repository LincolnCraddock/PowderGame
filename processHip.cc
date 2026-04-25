/*
 * Authors  : Lincoln Craddock, John Hershey
 * Date     : 2026-04-24
 * Professor: Dr. Gary Zoppetti
 * Class    : CMSC 476 Parallel Programming
 * Description: Description: test file for HIP
  from: https://rocm.docs.amd.com/projects/install-on-windows/en/develop/how-to/debugger-windows.html
  should be compiled with: hipcc example.cpp -o example.exe -gdwarf -Wl,-debug:dwarf -O3
 */
#include <hip/hip_runtime.h>
#include <assert.h>

__global__ void do_an_addition(int a, int b, int *out)
{
  *out = a + b;
}

int main ()
{
  int *result_ptr, result;
  /* Allocate memory for the device to write the result to. */
  hipError_t error = hipMalloc(&result_ptr, sizeof(int));
  assert(error == hipSuccess);

  /* Run `do_an_addition` on one block containing one HIP thread. */
  do_an_addition<<<dim3(1), dim3(1), 0, 0>>>(1, 2, result_ptr);

  /* Copy result from device to host. This acts as a
     synchronization point, waiting for the kernel dispatch to
     complete. */
  error = hipMemcpyDtoH(&result, result_ptr, sizeof(int));
  assert(error == hipSuccess);

  printf("result is %d\n", result);
  assert(result == 3);
  return 0;
}