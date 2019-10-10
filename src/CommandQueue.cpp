#include "CommandQueue.h"

namespace bdr
{
    CommandQueue::CommandQueue(D3D12_COMMAND_LIST_TYPE type) :
        m_pQueue{ nullptr },
        m_type{ type },
        m_pFence{ nullptr },
        m_allocatorPool{ m_type },
        m_nextFenceValue{ (static_cast<uint64_t>(type) << 56) + 1 },
        m_lastCompletedValue{ (static_cast<uint64_t>(type) << 56) + 1 },
        m_fenceEventHandle{ INVALID_HANDLE_VALUE }
    {
    }

    CommandQueue::~CommandQueue()
    {
        shutdown();
    }

    void CommandQueue::init(ID3D12Device* pDevice)
    {
        ASSERT(pDevice != nullptr);
        ASSERT(!isReady());
        ASSERT(m_allocatorPool.size() == 0);
        
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = m_type;

        ASSERT_SUCCEEDED(pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pQueue)));
        m_pQueue->SetName(L"CommandQueue::m_pQueue");

        ASSERT_SUCCEEDED(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence)));
        m_pFence->SetName(L"CommandQueue::m_pFence");
        m_pFence->Signal(m_lastCompletedValue);

        m_fenceEventHandle = CreateEventEx(NULL, false, false, EVENT_ALL_ACCESS);
        ASSERT(m_fenceEventHandle != INVALID_HANDLE_VALUE);

        m_allocatorPool.init(pDevice);
        ASSERT(isReady());
    }

    void CommandQueue::shutdown()
    {
        CloseHandle(m_fenceEventHandle);

        m_pFence->Release();
        m_pFence = nullptr;
        m_pQueue->Release();
        m_pQueue = nullptr;
    }

    bool CommandQueue::isFenceComplete(const uint64_t fenceValue)
    {
        if (fenceValue > m_lastCompletedValue) {
            m_lastCompletedValue = getCurrentFenceValue();
        }
        return m_lastCompletedValue >= fenceValue;
    }

    void CommandQueue::insertWait(const uint64_t fenceValue)
    {
        m_pQueue->Wait(m_pFence, fenceValue);
    }

    void CommandQueue::insertWaitOnOtherQueueFence(const CommandQueue* otherQueue, const uint64_t fenceValue)
    {
        m_pQueue->Wait(otherQueue->m_pFence, fenceValue);
    }

    void CommandQueue::insertWaitOnOtherQueue(const CommandQueue* otherQueue)
    {
        m_pQueue->Wait(otherQueue->m_pFence, otherQueue->m_nextFenceValue - 1);
    }

    void CommandQueue::waitForFence(const uint64_t fenceValue)
    {
        if (isFenceComplete(fenceValue)) {
            return;
        }

        {
            std::lock_guard<std::mutex> lockGuard(m_eventMutex);

            m_pFence->SetEventOnCompletion(fenceValue, m_fenceEventHandle);
            WaitForSingleObjectEx(m_fenceEventHandle, INFINITE, false);
            m_lastCompletedValue = fenceValue;
        }
    }

    uint64_t CommandQueue::executeCommandList(ID3D12CommandList* commandList)
    {
        ID3D12CommandList* ppCommandLists[]{ commandList };
        m_pQueue->ExecuteCommandLists(1, ppCommandLists);

        std::lock_guard<std::mutex> lockGuard(m_fenceMutex);
        ASSERT_SUCCEEDED(m_pQueue->Signal(m_pFence, m_nextFenceValue));
        return m_nextFenceValue++;
    }

    // COMMAND QUEUE MANAGER
    CommandQueueManager::CommandQueueManager() :
        m_graphicsQueue{ D3D12_COMMAND_LIST_TYPE_DIRECT },
        m_computeQueue{ D3D12_COMMAND_LIST_TYPE_COMPUTE },
        m_copyQueue{ D3D12_COMMAND_LIST_TYPE_COPY },
        m_pDevice{ nullptr }
    { }
    
    void CommandQueueManager::init(ID3D12Device* pDevice)
    {
        m_pDevice = pDevice;
        m_graphicsQueue.init(pDevice);
        m_computeQueue.init(pDevice);
        m_copyQueue.init(pDevice);
    }
    
    void CommandQueueManager::createNewCommandList(D3D12_COMMAND_LIST_TYPE type, ID3D12GraphicsCommandList** list, ID3D12CommandAllocator** allocator)
    {
        switch (type) {
        case D3D12_COMMAND_LIST_TYPE_DIRECT: *allocator = m_graphicsQueue.requestAllocator(); break;
        case D3D12_COMMAND_LIST_TYPE_COMPUTE: *allocator = m_computeQueue.requestAllocator(); break;
        case D3D12_COMMAND_LIST_TYPE_COPY: *allocator = m_copyQueue.requestAllocator(); break;
        }

        ASSERT_SUCCEEDED(m_pDevice->CreateCommandList(1, type, *allocator, nullptr, IID_PPV_ARGS(list)));
        (*list)->SetName(L"CommandList");
    }
}
