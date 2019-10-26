#include "LinearAllocator.h"
#include "CommandQueue.h"
#include <thread>
#include "Math/Common.h"

namespace bdr
{
    LinearAllocatorType LinearPageAllocationManager::sm_autoType = LinearAllocatorType::kGpuExclusive;

    LinearPageAllocationManager::LinearPageAllocationManager(ID3D12Device* pDevice, CommandQueueManager* pCmdQueueManager) :
        m_pDevice{ pDevice },
        m_pCmdQueueManager{ pCmdQueueManager },
        m_type{ sm_autoType },
        m_pagePool{},
        m_retiredPages{},
        m_deletionQueue{}
    {
        ASSERT(sm_autoType != LinearAllocatorType::kCpuWritable);
        sm_autoType = LinearAllocatorType::kCpuWritable;
    }

    LinearPageAllocationManager::~LinearPageAllocationManager()
    {
        destroy();
    }

    LinearPageAllocation LinearPageAllocationManager::requestPage()
    {
        std::lock_guard<std::mutex> lockGuard(m_mutex);

        LinearPageAllocation page;
        bool isPageAvailable = false;

        // If there are any retired pages, check if they are no longer in use
        if (!m_retiredPages.empty()) {
            auto& retiredPagePair = m_retiredPages.front();
            if (m_pCmdQueueManager->isFenceComplete(retiredPagePair.first)) {
                isPageAvailable = true;
                page = retiredPagePair.second;
                m_retiredPages.pop();
            }
        }
        // We weren't able to find a retired page that was available
        if (!isPageAvailable) {
            page = createNewPage();
        }

        return page;
    }

    LinearPageAllocation LinearPageAllocationManager::createNewPage(size_t pageSize)
    {
        D3D12_HEAP_PROPERTIES heapProps;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC resourceDesc;
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = 0;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        D3D12_RESOURCE_STATES defaultUsage;

        if (m_type == LinearAllocatorType::kGpuExclusive) {
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
            resourceDesc.Width = pageSize == 0 ? kGpuAllocatorPageSize : pageSize;
            resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            defaultUsage = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
        else {
            heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
            resourceDesc.Width = pageSize == 0 ? kCpuAllocatorPageSize : pageSize;
            resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            defaultUsage = D3D12_RESOURCE_STATE_GENERIC_READ;
        }

        ID3D12Resource* pBuffer;
        ASSERT_SUCCEEDED(m_pDevice->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            defaultUsage,
            nullptr,
            IID_PPV_ARGS(&pBuffer)
        ));

        pBuffer->SetName(L"Linear Page Allocation");

        m_pagePool.emplace_back(pBuffer);
        return m_pagePool.back();
    }

    void LinearPageAllocationManager::discardPages(uint64_t fenceValue, const std::vector<LinearPageAllocation>& pages)
    {
        std::lock_guard<std::mutex> lockGuard{ m_mutex };
        for (const auto& page : pages) {
            m_retiredPages.push(std::make_pair(fenceValue, page));
        }
    }

    void LinearPageAllocationManager::freeLargePages(uint64_t fenceValue, const std::vector<LinearPageAllocation>& largePages)
    {
        std::lock_guard<std::mutex> lockGuard{ m_mutex };

        while (!m_deletionQueue.empty()) {
            auto pagePair = m_deletionQueue.front();
            if (m_pCmdQueueManager->isFenceComplete(pagePair.first)) {
                pagePair.second.destroy();
                m_deletionQueue.pop();
            }
            else {
                // We've reached a page that is still in use!
                break;
            }
        }

        for (const auto& page : largePages) {
            m_deletionQueue.push(std::make_pair(fenceValue, page));
        }
    }

    void LinearPageAllocationManager::destroy()
    {
        for (auto& page : m_pagePool) {
            page.destroy();
        }
        m_pagePool.clear();
    }


    DynAlloc LinearAllocator::Allocate(size_t sizeInBytes, size_t alignment)
    {
        const size_t alignmentMask = alignment - 1;

        // Assert that it's a power of two
        ASSERT((alignmentMask & alignment) == 0);

        // Align the allocation
        const size_t alignedSize = Math::AlignUpWithMask(sizeInBytes, alignmentMask);

        if (alignedSize > m_pageSize) {
            return AllocateLargePage(sizeInBytes);
        }

        m_currentOffset = Math::AlignUp(m_currentOffset, alignment);

        // We've run out of space in our current page
        if (m_currentOffset + alignedSize > m_pageSize) {
            ASSERT(m_currentPage.isValid())
            m_retiredPages.push_back(m_currentPage);
            m_currentPage = LinearPageAllocation{};
        }

        // If we've never allocated a page, or run out of space, request a new one
        if (!m_currentPage.isValid()) {
            m_currentPage = sm_PageManager[m_type].requestPage();
            m_currentOffset = 0;
            ASSERT(m_currentPage.isValid());
        }

        DynAlloc ret = m_currentPage.getAlloc(m_currentOffset, alignedSize);
        
        m_currentOffset += alignedSize;
        return ret;
    }

    void LinearAllocator::CleanupUsedPages(uint64_t fenceValue)
    {
        // Haven't actually allocated any pages yet
        if (!m_currentPage.isValid()) {
            return;
        }

        m_retiredPages.push_back(m_currentPage);
        m_currentPage = LinearPageAllocation{};
        m_currentOffset = 0;

        sm_PageManager[m_type].discardPages(fenceValue, m_retiredPages);
        m_retiredPages.clear();

        sm_PageManager[m_type].freeLargePages(fenceValue, m_largePageList);
        m_largePageList.clear();
    }

    DynAlloc LinearAllocator::AllocateLargePage(size_t sizeInBytes)
    {
        LinearPageAllocation oneOff = sm_PageManager[m_type].createNewPage(sizeInBytes);
        m_largePageList.push_back(oneOff);

        return oneOff.getAlloc(0, sizeInBytes);
    }
    
    DynAlloc LinearPageAllocation::getAlloc(size_t offset, size_t size)
    {
        DynAlloc alloc{ resource, offset, size};
        alloc.dataPtr = (uint8_t*)cpuVirtualAddress + offset;
        alloc.gpuAddress = gpuVirtualAddress + offset;
        return alloc;
    }
}
