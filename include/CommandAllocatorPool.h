#pragma once
#include <stdafx.h>
#include <vector>
#include <queue>
#include <mutex>

#include "dx_helpers.h"

namespace bdr
{
    // Based off the miniEngine example
    class CommandAllocatorPool
    {
    public:
        CommandAllocatorPool(const D3D12_COMMAND_LIST_TYPE commandListType) :
            m_listType{ commandListType },
            m_pDevice{ nullptr }
        { }

        ~CommandAllocatorPool()
        {
            shutdown();
        }

        void init(ID3D12Device* pDevice)
        {
            m_pDevice = pDevice;

        }
        void shutdown()
        {
            for (size_t i = 0; i < m_allocatorPool.size(); i++) {
                m_allocatorPool[i]->Release();
            }
            m_allocatorPool.clear();
        }

        ID3D12CommandAllocator* requestAllocator(uint64_t completedFenceValue)
        {
            std::lock_guard<std::mutex> lockGuard{ m_allocatorMutex };

            ID3D12CommandAllocator* pAllocator = nullptr;
            
            if (!m_readyAllocators.empty()) {
                auto& allocatorPair = m_readyAllocators.front();

                if (allocatorPair.first <= completedFenceValue) {
                    pAllocator = allocatorPair.second;
                    ThrowIfFailed(pAllocator->Reset());
                    m_readyAllocators.pop();
                }
            }
            
            if (pAllocator == nullptr) {
                ThrowIfFailed(m_pDevice->CreateCommandAllocator(m_listType, IID_PPV_ARGS(&pAllocator)));
                wchar_t name[32];
                swprintf(name, 32, L"CommandAllocator %zu", m_allocatorPool.size());
                pAllocator->SetName(name);
                m_allocatorPool.push_back(pAllocator);
            }

            return pAllocator;
        };

        void returnAllocator(const uint64_t fenceValue, ID3D12CommandAllocator* allocator)
        {
            std::lock_guard<std::mutex> lockGuard{ m_allocatorMutex };

            m_readyAllocators.push(std::make_pair(fenceValue, allocator));
        }

        inline size_t size() const
        {
            return m_allocatorPool.size();
        }


    private:
        const D3D12_COMMAND_LIST_TYPE m_listType;

        ID3D12Device* m_pDevice;
        std::vector<ID3D12CommandAllocator*> m_allocatorPool;
        std::queue<std::pair<uint64_t, ID3D12CommandAllocator*>> m_readyAllocators;
        std::mutex m_allocatorMutex;
    };

}