#pragma once
#include <mutex>
#include "dx_helpers.h"
#include "CommandAllocatorPool.h"
namespace bdr
{
    // Based off the MiniEngine Example
    // And Alex Tardif's Example: http://www.alextardif.com/D3D11To12P1.html
    class CommandQueue
    {
    public:
        CommandQueue(D3D12_COMMAND_LIST_TYPE type);
        ~CommandQueue();

        void init(ID3D12Device* pDevice);
        void shutdown();

        inline bool isReady()
        {
            return m_pQueue != nullptr;
        }

        inline ID3D12CommandQueue* get()
        {
            return m_pQueue;
        }

        bool isFenceComplete(const uint64_t fenceValue);
        // Note that these functions are NOT CPU blocking!
        void insertWait(const uint64_t fenceValue);
        void insertWaitOnOtherQueueFence(const CommandQueue* otherQueue, const uint64_t fenceValue);
        void insertWaitOnOtherQueue(const CommandQueue* otherQueue);
        // These functions are CPU blocking
        void waitForFence(const uint64_t fenceValue);
        inline void waitForIdle()
        {
            waitForFence(m_nextFenceValue - 1);
        };

        uint64_t executeCommandList(ID3D12CommandList* commandList);

        inline ID3D12CommandAllocator* requestAllocator()
        {
            uint64_t completedValue = m_pFence->GetCompletedValue();
            return m_allocatorPool.requestAllocator(completedValue);
        }

        inline void returnAllocator(const uint64_t fenceValue, ID3D12CommandAllocator* allocator)
        {
            m_allocatorPool.returnAllocator(fenceValue, allocator);
        }

        inline uint64_t getNextFenceValue() {
            return m_nextFenceValue;
        }
        
        inline uint64_t getCurrentFenceValue() const {
            uint64_t completedValue = m_pFence->GetCompletedValue();
            return m_lastCompletedValue > completedValue ? m_lastCompletedValue : completedValue;
        }
        
        ID3D12CommandQueue* m_pQueue;
        const D3D12_COMMAND_LIST_TYPE m_type;
        ID3D12Fence* m_pFence;

    private:
        CommandAllocatorPool m_allocatorPool;

        std::mutex m_fenceMutex;
        std::mutex m_eventMutex;

        uint64_t m_nextFenceValue;
        uint64_t m_lastCompletedValue;
        HANDLE m_fenceEventHandle;
    };

    class CommandQueueManager
    {
    public:
        CommandQueueManager();
        
        void init(ID3D12Device* pDevice);
        void createNewCommandList(
            D3D12_COMMAND_LIST_TYPE type,
            ID3D12GraphicsCommandList** list,
            ID3D12CommandAllocator** allocator);
        
        CommandQueue m_graphicsQueue;
        CommandQueue m_computeQueue;
        CommandQueue m_copyQueue;
        
    private:
        ID3D12Device* m_pDevice;

    };
}

