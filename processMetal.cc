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
MTL::Buffer* bufWorld;
MTL::Buffer* bufNewWorld;
MTL::Size threadsPerGroup;
MTL::Size threadsPerGrid;

std::vector<Data>
process_powder (std::vector<Data>& world, unsigned H, unsigned W)
{
    Data* bufWorldPtr = (Data*)bufWorld->contents ();
    Data* bufNewWorldPtr = (Data*)bufNewWorld->contents ();
    for (uint y = 0; y < H; ++y)
        for (uint x = 0; x < W; ++x)
        {
            uint i           = y * W + x;
            bufWorldPtr[i] = world[i];
            bufNewWorldPtr[i] = { EMPTY, 0 };
        }

    /* Create a command buffer and encoder */
    MTL::CommandBuffer* cmdBuf = queue->commandBuffer ();
    MTL::ComputeCommandEncoder* encoder = cmdBuf->computeCommandEncoder ();

    /* Write commands to the buffer via the encoder */
    encoder->setComputePipelineState (pipeline);

    /* Write data to the buffer via the encoder */
    // the indices (0 and 1) represent the parameters the buffers supply
    // the offsets (all 0s) are for if you only want one element of an array to be supplied
    encoder->setBuffer (bufWorld, 0, 0);
    encoder->setBuffer (bufNewWorld, 0, 1);

    // W × H grid of threads, one per cell
    encoder->dispatchThreads (threadsPerGrid, threadsPerGroup);
    encoder->endEncoding ();
    encoder->release ();

    /* Finally, run the commands encoded in the command buffer on the GPU */
    cmdBuf->commit ();

    /* Wait until the commands have finished */
    cmdBuf->waitUntilCompleted ();
    cmdBuf->release ();

    /* Return the results from the output buffer */
    std::vector<Data> newWorld (bufNewWorldPtr, bufNewWorldPtr + H * W);
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

        kernel void process_powder (
            device const Data* data [[ buffer (0) ]],
            device Data* result     [[ buffer (1) ]],
            uint2 gid               [[ thread_position_in_grid ]]
        ) {
            uint idx = gid.y + %i * gid.x;
            switch (data[idx].type)
            {
                case DIRT:
                {
                    if (gid.y > 0 && data[idx - 1].type == EMPTY)
                    {
                        result[idx - 1] = { DIRT, 0 };
                    }
                    else
                    {
                        result[idx] = { DIRT, 0 };
                    }
                    break;
                }
                case STONE:
                {
                    unsigned dy = 1;
                    while (dy <= data[idx].dy + 1 && gid.y >= dy &&
                           data[idx - dy].type == EMPTY)
                        ++dy;
                    --dy;
                    if (dy > 0)
                    {
                        result[idx - dy] = { STONE, dy };
                    }
                    else
                    {
                        result[idx] = { STONE, 0 };
                    }
                    break;
                }
            }
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
    NS::String* fnName = NS::String::string ("process_powder", NS::UTF8StringEncoding);
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

    bufWorld = device->newBuffer (inputBufferSize,  MTL::ResourceStorageModeShared);
    bufNewWorld = device->newBuffer (outputBufferSize, MTL::ResourceStorageModeShared);

    /* Calculate the maximum threads per threadgroup based on the thread execution width */
    NS::UInteger w            = pipeline->threadExecutionWidth ();
    NS::UInteger h            = pipeline->maxTotalThreadsPerThreadgroup () / w;
    MTL::Size threadsPerGroup = MTL::Size (w, h, 1);
    MTL::Size threadsPerGrid  = MTL::Size (W, H, 1);

    /* Create a command queue */
    queue = device->newCommandQueue ();
}