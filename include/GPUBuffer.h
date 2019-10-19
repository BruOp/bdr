#pragma once
#include "CommandQueue.h"
#include "GPUResource.h"
#include <array>


namespace bdr
{
    struct GPUBuffer : GPUResource
    {
        GPUBuffer() = default;
        GPUBuffer(uint32_t numElements, uint32_t elementSize, size_t bufferSize) :
            GPUResource{ },
            numElements{ numElements },
            elementSize{ elementSize },
            bufferSize{ bufferSize }
        { }
        GPUBuffer(ID3D12Resource* resource, uint32_t numElements, uint32_t elementSize, size_t bufferSize) :
            GPUResource{ resource },
            numElements{ numElements },
            elementSize{ elementSize },
            bufferSize{ bufferSize }
        { }

        uint32_t numElements = 0;
        uint32_t elementSize = 0;
        size_t bufferSize = 0;
    };

    class GPUBufferManager
    {
    public:
        constexpr static size_t STAGING_MAX = 256;

        GPUBufferManager() = default;
        ~GPUBufferManager()
        {
            shutdown();
        };

        void init(ID3D12Device* pDevice, CommandQueueManager* pCmdQueueManager);
        void shutdown();

        // This is CPU blocking
        void reset();

        // Executes copy commands on the copy queue, and also adds the resource barriers to the input
        // graphicsCmdList. This function must be called before using any of the resources returned
        // from the `createOnGPU` function
        void execute(bool waitForCompletion = false);

        GPUBuffer createOnGPU(
            const std::wstring& name,
            const uint32_t numElements,
            const uint32_t elementSize,
            const void* userData = nullptr
        );

        bool isComplete() const;

        // Don't own these
        ID3D12Device* m_device = nullptr;
        CommandQueueManager* m_cmdQueueManager = nullptr;
        // Do own these
        ID3D12GraphicsCommandList* m_commandList = nullptr;
        ID3D12CommandAllocator* m_commandAllocator = nullptr;

        std::array<GPUResource, STAGING_MAX> m_stagingBuffers;
        std::array<D3D12_RESOURCE_BARRIER, STAGING_MAX> m_resourceBarriers;
        size_t m_stagedEndIdx = 0;
        bool m_isReady = false;
        uint64_t m_currentFence = 0;
    };
}

