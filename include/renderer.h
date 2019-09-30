#pragma once
#include <cstdint>
#include "stdafx.h"

using Microsoft::WRL::ComPtr;

namespace bdr
{
    // WIP
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        uint32_t color;
    };
    
    struct RenderConfig
    {
        uint16_t width;
        uint16_t height;
        std::wstring assetsPath;
    };

    class Renderer
    {
    public:
        Renderer(const RenderConfig& renderConfig);
        ~Renderer();
        void init(HWND windowHandle);
        void initPipeline();
        void initAssets();

        void onRender();

        inline std::wstring GetAssetFullPath(LPCWSTR assetFileName)
        { 
            return m_renderConfig.assetsPath + assetFileName;
        };

        RenderConfig m_renderConfig;
        HWND m_windowHandle = nullptr;

    private:
        static constexpr uint32_t FRAME_COUNT = 2u;

        void populateCommandList();
        void waitForGPU();
        void moveToNextFrame();
        void getHardwareAdapter(_In_ IDXGIFactory2* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter);

        CD3DX12_VIEWPORT m_viewport;
        CD3DX12_RECT m_scissorRect;
        ComPtr<ID3D12Device> m_device;
        ComPtr<ID3D12CommandQueue> m_graphicsQueue;
        ComPtr<ID3D12CommandAllocator> m_commandAllocators[FRAME_COUNT];
        ComPtr<ID3D12GraphicsCommandList> m_commandList;
        ComPtr<IDXGISwapChain3> m_swapChain;
        ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
        ComPtr<ID3D12Resource> m_renderTargets[FRAME_COUNT];
        ComPtr<ID3D12RootSignature> m_rootSignature;
        ComPtr<ID3D12PipelineState> m_pipelineState;
        ComPtr<ID3D12Fence> m_fence;    
        HANDLE m_fenceEvent;
        uint64_t m_fenceValues[FRAME_COUNT];
        uint32_t m_frameIndex = 0;

        ComPtr<ID3D12Resource> m_vertexBuffer;
        D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

        uint32_t m_rtvDescriptorSize = 0;

        float m_aspectRatio;
    };
}