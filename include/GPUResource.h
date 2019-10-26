#pragma once
#include <d3d12.h>
#include "d3dx12.h"
#include "dx_helpers.h"


namespace bdr
{
    struct GPUResource
    {
        GPUResource() = default;
        GPUResource(ID3D12Resource* resource) :
            pResource(resource),
            usageState(D3D12_RESOURCE_STATE_COMMON),
            transitioningState(D3D12_RESOURCE_STATE_COMMON),
            gpuVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL)
        { }
        GPUResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES currentState) :
            pResource(resource),
            usageState(currentState),
            transitioningState(currentState),
            gpuVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL)
        { }

        inline void destroy()
        {
            pResource->Release();
            pResource = nullptr;
            gpuVirtualAddress = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
        }

        inline ID3D12Resource* get()
        {
            return pResource;
        }

        inline const ID3D12Resource* get() const
        {
            return pResource;
        }

        inline ID3D12Resource** getPPtr()
        {
            return &pResource;
        }

        inline ID3D12Resource* operator->()
        {
            return pResource;
        }

        inline bool isValid() { return pResource == nullptr; }


        ID3D12Resource* pResource = nullptr;
        D3D12_RESOURCE_STATES usageState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES transitioningState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_GPU_VIRTUAL_ADDRESS gpuVirtualAddress = D3D12_GPU_VIRTUAL_ADDRESS_NULL;
    };
}
