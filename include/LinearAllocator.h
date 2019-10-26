//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//
// Description:  This is a dynamic graphics memory allocator for DX12.  It's designed to work in concert
// with the CommandContext class and to do so in a thread-safe manner.  There may be many command contexts,
// each with its own linear allocators.  They act as windows into a global memory pool by reserving a
// context-local memory page.  Requesting a new page is done in a thread-safe manner by guarding accesses
// with a mutex lock.
//
// When a command context is finished, it will receive a fence ID that indicates when it's safe to reclaim
// used resources.  The CleanupUsedPages() method must be invoked at this time so that the used pages can be
// scheduled for reuse after the fence has cleared.

#pragma once
#include <mutex>
#include <vector>
#include <queue>

#include "dx_helpers.h"
#include "GPUResource.h"


namespace bdr
{
    class CommandQueueManager;

    constexpr uint64_t DEFAULT_ALIGN = 256u;

    struct DynAlloc
    {
        DynAlloc(GPUResource& resource, size_t offset, size_t size) :
            resource{ resource }, offset{ offset }, size{ size }, dataPtr{ nullptr }, gpuAddress{} {};

        GPUResource& resource;
        size_t offset;
        size_t size;
        void* dataPtr = nullptr; // CPU writable address
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddress;
    };


    class LinearPageAllocation
    {
    public:
        friend class LinearPageAllocationManager;
        friend class LinearAllocator;

        LinearPageAllocation() = default;
        LinearPageAllocation(ID3D12Resource* pResource) :
            resource{ pResource },
            cpuVirtualAddress{ nullptr },
            gpuVirtualAddress{ pResource->GetGPUVirtualAddress() }
        {
            map();
        }

        inline bool isValid()
        {
            return resource.isValid();
        }

    protected:

        DynAlloc getAlloc(size_t offset, size_t size);

        void destroy()
        {
            unmap();
            resource.destroy();
        }

        void map()
        {
            if (cpuVirtualAddress != nullptr) {
                resource->Map(0, nullptr, &cpuVirtualAddress);
            }
        }

        void unmap()
        {
            if (cpuVirtualAddress != nullptr) {
                resource->Unmap(0, nullptr);
                cpuVirtualAddress = nullptr;
            }
        }

        GPUResource resource;
        void* cpuVirtualAddress = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpuVirtualAddress;
    };



    enum LinearAllocatorType
    {
        kInvalidAllocator = -1,

        kGpuExclusive = 0,        // DEFAULT   GPU-writeable (via UAV)
        kCpuWritable = 1,        // UPLOAD CPU-writeable (but write combined)

        kNumAllocatorTypes
    };

    static constexpr size_t kGpuAllocatorPageSize = 0x10000;    // 64K
    static constexpr size_t kCpuAllocatorPageSize = 0x200000;    // 2MB


    class LinearPageAllocationManager
    {
    public:
        LinearPageAllocationManager(ID3D12Device* pDevice, CommandQueueManager* pCmdQueueManager);

        LinearPageAllocationManager(const LinearPageAllocationManager&) = delete;
        LinearPageAllocationManager& operator=(const LinearPageAllocationManager&) = delete;
        LinearPageAllocationManager(LinearPageAllocationManager&&) = delete;
        LinearPageAllocationManager& operator=(LinearPageAllocationManager&&) = delete;

        ~LinearPageAllocationManager();

        LinearPageAllocation requestPage();
        LinearPageAllocation createNewPage(size_t pageSize = 0);

        // Discarded pages get recycled when new requests come in
        void discardPages(uint64_t fenceValue, const std::vector<LinearPageAllocation>& pages);

        // These are simply released and destroy
        void freeLargePages(uint64_t fenceValue, const std::vector<LinearPageAllocation>& pages);

        void destroy();

    private:
        // This is used to basically ensure that only two page allocators are ever created
        static LinearAllocatorType sm_autoType;

        ID3D12Device* m_pDevice = nullptr;
        CommandQueueManager* m_pCmdQueueManager = nullptr;

        LinearAllocatorType m_type;
        // m_pagePool contains both regular and "large" pages.
        std::vector<LinearPageAllocation> m_pagePool;
        // List of pages to recycle, does not include "large" pages
        std::queue<std::pair<uint64_t, LinearPageAllocation>> m_retiredPages;
        // This only contians "large" pages
        std::queue<std::pair<uint64_t, LinearPageAllocation>> m_deletionQueue;
        std::mutex m_mutex;
    };


    class LinearAllocator
    {
    public:
        LinearAllocator(LinearAllocatorType type) :
            m_type{ type },
            m_pageSize{ type == LinearAllocatorType::kGpuExclusive ? kGpuAllocatorPageSize : kCpuAllocatorPageSize },
            m_currentOffset{ ~size_t{0} },
            m_currentPage{ nullptr },
            m_retiredPages{},
            m_largePageList{}
        {
            ASSERT(m_type > LinearAllocatorType::kInvalidAllocator&& m_type < LinearAllocatorType::kNumAllocatorTypes);
        };

        DynAlloc Allocate(size_t sizeInBytes, size_t alignment = DEFAULT_ALIGN);

        void CleanupUsedPages(uint64_t fenceValue);

    private:

        DynAlloc AllocateLargePage(size_t sizeInBytes);

        static LinearPageAllocationManager sm_PageManager[2];

        LinearAllocatorType m_type;
        size_t m_pageSize;
        size_t m_currentOffset;
        LinearPageAllocation m_currentPage;
        std::vector<LinearPageAllocation> m_retiredPages;
        std::vector<LinearPageAllocation> m_largePageList;
    };
}

