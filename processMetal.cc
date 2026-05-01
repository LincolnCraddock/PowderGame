/*
 * Authors  : Lincoln Craddock
 * Date     : 2026-04-24
 * Professor: Dr. Gary Zoppetti
 * Class    : CMSC 476 Parallel Programming
 * Description: Uses Metal to update a 2-dimensional grid of data points
 * updating positions to simulate gravity
 */

#include "PowderGame.h"

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include "metal-cpp/Metal/Metal.hpp"

#include <iostream>

MTL::CommandQueue* queue;
MTL::ComputePipelineState* pipeline;
MTL::Buffer* bufPairs;
MTL::Buffer* bufC;
MTL::Size threadsPerGroup;
MTL::Size threadsPerGrid;

std::vector<Data>
process_powder (std::vector<Data>& world, unsigned H, unsigned W)
{
    std::vector<Data> newWorld (H * W, { EMPTY, 0 });

    /* Create a command buffer and encoder */
    MTL::CommandBuffer* cmdBuf = queue->commandBuffer ();
    MTL::ComputeCommandEncoder* encoder = cmdBuf->computeCommandEncoder ();

    /* Write commands to the buffer via the encoder */
    encoder->setComputePipelineState (pipeline);

    /* Write data to the buffer via the encoder */
    // the indices (0 and 1) represent the parameters the buffers supply
    // the offsets (all 0s) are for if you only want one element of an array to be supplied
    encoder->setBuffer (bufPairs, 0, 0);
    encoder->setBuffer (bufC,     0, 1);

    // W × H grid of threads, one per cell
    encoder->dispatchThreads (threadsPerGrid, threadsPerGroup);
    encoder->endEncoding ();
    encoder->release ();

    /* Finally, run the commands encoded in the command buffer on the GPU */
    cmdBuf->commit ();

    /* Wait until the commands have finished */
    cmdBuf->waitUntilCompleted ();
    cmdBuf->release ();

    /* Read the results from the output buffer, bufC */
    return newWorld;
}


extern void
set_up_processing ()
{
    /* Find a GPU */
    MTL::Device* device = MTL::CreateSystemDefaultDevice();
    if (!device)
    {
        std::cerr << "Metal not supported" << std::endl;
        return;
    }

    /* Metal function */
    char shaderSrc[1024];
    snprintf(shaderSrc, 1024, R"(
        #include <metal_stdlib>
        using namespace metal;

        enum PowderType
        {
            EMPTY,
            DIRT,
            STONE,
        };

        struct Data
        {
        PowderType type = EMPTY;
        // only computed for stone
        unsigned dy = 0;
        };

        kernel void add_pairs (
            device const Data* data [[ buffer (0) ]],
            device Data* result          [[ buffer (1) ]],
            uint2 gid                     [[ thread_position_in_grid ]]
        ) {
            uint index = gid.y + %i * gid.x;
            result[index] = pairs[index].a + pairs[index].b;
        }
    )", H);

    /* Get a ref to the metal function */
    NS::Error* error = nullptr;
    NS::String* src = NS::String::string(shaderSrc, NS::UTF8StringEncoding);
    MTL::CompileOptions* opts = MTL::CompileOptions::alloc ()->init ();
    MTL::Library* library = device->newLibrary (src, opts, &error);
    if (!library)
    {
        std::cerr << "Shader error: " << error->localizedDescription ()->utf8String () << std::endl;
        return;
    }

    /* Prepare a metal pipeline */
    NS::String* fnName = NS::String::string ("add_pairs", NS::UTF8StringEncoding);
    MTL::Function* function = library->newFunction (fnName);
    // A 'compute' pipeline runs a single 'compute' function
    MTL::ComputePipelineState* pipeline = device->newComputePipelineState (function, &error);
    if (!pipeline)
    {
        std::cerr << "Pipeline error: " << error->localizedDescription ()->utf8String () << std::endl;
        return;
    }

    /* Create data buffers and load data into them */
    const size_t count = W * H;
    const size_t inputBufferSize  = count * sizeof(Data);
    const size_t outputBufferSize = count * sizeof(Data);

    std::vector<Data> pairs (count);
    for (size_t y = 0; y < H; ++y)
        for (size_t x = 0; x < W; ++x)
        {
            size_t i      = y * W + x;
            pairs[i].type = EMPTY;
            pairs[i].dy = 0;
        }

    bufPairs = device->newBuffer (pairs.data (), inputBufferSize,  MTL::ResourceStorageModeShared);
    bufC     = device->newBuffer (outputBufferSize, MTL::ResourceStorageModeShared);

    /* Calculate the maximum threads per threadgroup based on the thread execution width */
    NS::UInteger w            = pipeline->threadExecutionWidth ();
    NS::UInteger h            = pipeline->maxTotalThreadsPerThreadgroup () / w;
    MTL::Size threadsPerGroup = MTL::Size (w, h, 1);
    MTL::Size threadsPerGrid  = MTL::Size (W, H, 1);

    /* Create a command queue */
    queue = device->newCommandQueue ();

     // run the GPU task immediately the 1st time
    tLast = std::chrono::steady_clock::now () - std::chrono::seconds (1);
}