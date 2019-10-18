#include "GPUBuffer.h"
#include <dx_helpers.h>

using Microsoft::WRL::ComPtr;

namespace bdr
{
    GPUBuffer GPUBufferManager::createOnGPU(
        const std::wstring& name,
        const uint32_t numElements,
        const uint32_t elementSize,
        const void* userData
    )
    {
        GPUBuffer buffer{
            numElements,
            elementSize,
            numElements * elementSize
        };

        ASSERT_SUCCEEDED(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT, 1, 1),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(buffer.bufferSize),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&buffer.resource.m_pResource)
        ));

        buffer.resource->SetName(name.c_str());
        if (userData != nullptr) {
            ASSERT(isComplete());
            if (m_stagedEndIdx == STAGING_MAX - 1) {
                execute(true);
                reset();
            };

            // Create staging buffer
            D3D12_RESOURCE_BARRIER barrierDesc = m_resourceBarriers[m_stagedEndIdx];
            GPUResource& stagingBuffer = m_stagingBuffers[m_stagedEndIdx++];
            ID3D12Resource** pResource = stagingBuffer.getPPtr();
            ASSERT_SUCCEEDED(m_device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(buffer.bufferSize),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(pResource)
            ));

            // Copy our memory
            uint8_t* pDataBegin = nullptr;
            CD3DX12_RANGE readRange{ 0, 0 };
            ASSERT_SUCCEEDED(stagingBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pDataBegin)));
            memcpy(pDataBegin, userData, buffer.bufferSize);
            stagingBuffer->Unmap(0, nullptr);

            // Issue the copy call to the command list
            m_commandList->CopyBufferRegion(buffer.resource.get(), 0, stagingBuffer.get(), 0, buffer.bufferSize);

            // Create transition barriers for our resources
            // NOTE: Not used, since these resources should decay to D3D12_RESOURCE_STATE_COMMON after the copy
            //barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            //barrierDesc.Transition.pResource = buffer.resource.get();
            //barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            //barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            //barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
        }

        return buffer;
    }

    bool GPUBufferManager::isComplete() const
    {
        return m_cmdQueueManager->m_copyQueue.isFenceComplete(m_currentFence);
    }

    void GPUBufferManager::init(ID3D12Device* pDevice, CommandQueueManager* pCmdQueueManager)
    {
        ASSERT(!m_isReady);
        m_device = pDevice;
        m_cmdQueueManager = pCmdQueueManager;

        m_cmdQueueManager->createNewCommandList(D3D12_COMMAND_LIST_TYPE_COPY, &m_commandList, &m_commandAllocator);
        m_isReady = true;
    }

    void GPUBufferManager::shutdown()
    {
        if (!m_isReady) {
            return;
        }
        
        for (size_t i = 0; i < m_stagedEndIdx; i++) {
            m_stagingBuffers[i]->Release();
        }
        m_stagedEndIdx = 0;
    }

    void GPUBufferManager::reset()
    {
        ASSERT(isComplete());
        if (m_stagedEndIdx == 0) {
            return;
        }
        m_commandAllocator = m_cmdQueueManager->m_copyQueue.requestAllocator();
        ASSERT_SUCCEEDED(m_commandList->Reset(m_commandAllocator, nullptr));

        for (size_t i = 0; i < m_stagedEndIdx; i++) {
            m_stagingBuffers[i]->Release();
        }
        m_stagedEndIdx = 0;
    }


    void GPUBufferManager::execute(bool waitForCompletion)
    {
        ASSERT(isComplete());
        // Don't execute if we don't have any resources to copy/transition
        if (m_stagedEndIdx == 0) {
            return;
        }

        CommandQueue& copyQueue = m_cmdQueueManager->m_copyQueue;
        CommandQueue& graphicsQueue = m_cmdQueueManager->m_graphicsQueue;

        // First deal with executing the copy commands
        // Our command list is already populated through calls to createOnGPU
        ASSERT_SUCCEEDED(m_commandList->Close());
        m_currentFence = copyQueue.executeCommandList(m_commandList);
        
        copyQueue.returnAllocator(m_currentFence, m_commandAllocator);
        m_commandAllocator = nullptr;

        // Now deal with Resource Barriers on the graphics queue
        graphicsQueue.insertWaitOnOtherQueueFence(&copyQueue, m_currentFence);

        if (waitForCompletion) {
            copyQueue.waitForFence(m_currentFence);
        }
    }
    
    void GPUBuffer::destroy()
    {
        resource.destroy();
        bufferSize = 0;
        numElements = 0;
        elementSize = 0;
    }
}