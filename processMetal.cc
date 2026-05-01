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
#include <span>

// Make these global or static so they persist
static MTL::Device* device = nullptr;
static MTL::CommandQueue* queue = nullptr;
static MTL::ComputePipelineState* pipeline = nullptr;
static MTL::Buffer* bufWorld = nullptr;
static MTL::Buffer* bufNewWorld = nullptr;
static MTL::Size threadsPerGroup;
static MTL::Size threadsPerGrid;

std::span<Data>
process_powder ()
{
    Data* bufNewWorldPtr = (Data*)bufNewWorld->contents ();
    for (size_t i = 0; i < H * W; ++i)
        bufNewWorldPtr[i] = {EMPTY, 0};

    /* Create a command buffer and encoder */
    MTL::CommandBuffer* cmdBuf = queue->commandBuffer();
    if (!cmdBuf) {
        std::cerr << "Failed to create command buffer" << std::endl;
        return {};
    }
    
    MTL::ComputeCommandEncoder* encoder = cmdBuf->computeCommandEncoder();
    if (!encoder) {
        std::cerr << "Failed to create compute encoder" << std::endl;
        cmdBuf->release();
        return {};
    }

    /* Write commands to the buffer via the encoder */
    encoder->setComputePipelineState(pipeline);

    /* Write data to the buffer via the encoder */
    encoder->setBuffer(bufWorld, 0, 0);
    encoder->setBuffer(bufNewWorld, 0, 1);

    // W × H grid of threads, one per cell
    encoder->dispatchThreads(threadsPerGrid, threadsPerGroup);
    encoder->endEncoding();

    /* Finally, run the commands encoded in the command buffer on the GPU */
    cmdBuf->commit();

    /* Wait until the commands have finished */
    cmdBuf->waitUntilCompleted();
    
    /* Return the results from the output buffer */
    // std::vector<Data> newWorld(bufNewWorldPtr, bufNewWorldPtr + H * W);
    MTL::Buffer* temp;
    temp = bufNewWorld;
    bufNewWorld = bufWorld;
    bufWorld = temp;
    
    cmdBuf->release();
    
    return {(Data*)bufWorld->contents(), H * W};
}

std::span<Data>
set_up_processing ()
{    
    /* Find a GPU */
    device = MTL::CreateSystemDefaultDevice();
    if (!device)
    {
        std::cerr << "Metal not supported" << std::endl;
        return {};
    }
    
    /* Metal function */
    char shaderSrc[10'000];
    snprintf(shaderSrc, 10'000, R"(
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
                    while (dy <= data[idx].dy + 1 && gid.y >= dy && data[idx - dy].type == EMPTY)
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
                default:
                {
                    ;// nothing
                }
            }
        }
    )", (int)H);

    /* Get a ref to the metal function */
    NS::Error* error = nullptr;
    NS::String* src = NS::String::string(shaderSrc, NS::UTF8StringEncoding);
    MTL::CompileOptions* opts = MTL::CompileOptions::alloc()->init();
    if (!opts) {
        std::cerr << "Failed to create compile options" << std::endl;
        return {};
    }
    
    MTL::Library* library = device->newLibrary(src, opts, &error);
    opts->release();
    
    if (!library)
    {
        std::cerr << "Shader error: " << (error ? error->localizedDescription()->utf8String() : "unknown") << std::endl;
        if (error) error->release();
        return {};
    }
    
    /* Prepare a metal pipeline */
    NS::String* fnName = NS::String::string("process_powder", NS::UTF8StringEncoding);
    MTL::Function* function = library->newFunction(fnName);
    fnName->release();
    
    if (!function) {
        std::cerr << "Failed to get function from library" << std::endl;
        library->release();
        return {};
    }
    
    // A 'compute' pipeline runs a single 'compute' function
    pipeline = device->newComputePipelineState(function, &error);
    function->release();
    
    if (!pipeline)
    {
        std::cerr << "Pipeline error: " << (error ? error->localizedDescription()->utf8String() : "unknown") << std::endl;
        if (error) error->release();
        library->release();
        return {};
    }
    library->release();

    /* Create data buffers and load data into them */
    const size_t count = W * H;
    const size_t bufferSize = count * sizeof(Data);

    bufWorld = device->newBuffer(bufferSize, MTL::ResourceStorageModeShared);
    bufNewWorld = device->newBuffer(bufferSize, MTL::ResourceStorageModeShared);
    
    if (!bufWorld || !bufNewWorld) {
        std::cerr << "Failed to create buffers" << std::endl;
        return {};
    }
    
    /* Calculate the maximum threads per threadgroup based on the thread execution width */
    NS::UInteger w = pipeline->threadExecutionWidth();
    NS::UInteger h = pipeline->maxTotalThreadsPerThreadgroup() / w;
    threadsPerGroup = MTL::Size(w, h, 1);
    threadsPerGrid = MTL::Size(W, H, 1);


    /* Create a command queue */
    queue = device->newCommandQueue();
    if (!queue) {
        std::cerr << "Failed to create command queue" << std::endl;
        return {};
    }

    return { (Data*)bufWorld->contents(), H * W };
}