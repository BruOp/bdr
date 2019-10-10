#pragma once
#include <d3d12.h>
#include "d3dx12.h"

namespace bdr
{
    struct GPUResource
    {
        inline ID3D12Resource* get()
        {
            return m_pResource;
        }

        inline const ID3D12Resource* get() const
        {
            return m_pResource;
        }

        inline ID3D12Resource** getPPtr()
        {
            return &m_pResource;
        }

        inline ID3D12Resource* operator->()
        {
            return m_pResource;
        }

        inline void destroy()
        {
            m_pResource->Release();
            m_pResource = nullptr;
        }
        
        ID3D12Resource* m_pResource;
        D3D12_RESOURCE_STATES m_UsageState;
        D3D12_RESOURCE_STATES m_TransitioningState;
        D3D12_GPU_VIRTUAL_ADDRESS m_GpuVirtualAddress;
    };
}

