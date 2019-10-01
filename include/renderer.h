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

    const Vertex cubeVertices[] = {
        { DirectX::XMFLOAT3(-0.5f, 0.5f, -0.5f), 0xff00ff00 }, // +Y (top face)
        { DirectX::XMFLOAT3(0.5f, 0.5f, -0.5f),  0xff00ffff },
        { DirectX::XMFLOAT3(0.5f, 0.5f,  0.5f),  0xffffffff },
        { DirectX::XMFLOAT3(-0.5f, 0.5f,  0.5f), 0xffffff00 },

        { DirectX::XMFLOAT3(-0.5f, -0.5f,  0.5f), 0xffff0000 }, // -Y (bottom face)
        { DirectX::XMFLOAT3(0.5f, -0.5f,  0.5f), 0xffff00ff },
        { DirectX::XMFLOAT3(0.5f, -0.5f, -0.5f), 0xff0000ff },
        { DirectX::XMFLOAT3(-0.5f, -0.5f, -0.5f), 0xff000000 },
    };

    const uint16_t cubeIndices[] = {
        0, 1, 2,
        0, 2, 3,
        2, 1, 5,
        1, 6, 5,
        0, 3, 4,
        0, 4, 7,
        0, 7, 1,
        1, 7, 6,
        3, 2, 4,
        2, 5, 4,
        4, 5, 7,
        5, 6, 7
    };

    //const Vertex cubeVertices[] = {
    //    { { 0.0f, 0.25f, 0.0f }, 0xff00ff00 },
    //    { { 0.25f, -0.25f, 0.0f }, 0xffff0000 },
    //    { { -0.25f, -0.25f, 0.0f }, 0xff0000ff },
    //};

    //const uint16_t cubeIndices[] = {
    //    0, 1, 2,
    //};

    struct Mesh
    {
        ComPtr<ID3D12Resource> vertexBuffer;
        ComPtr<ID3D12Resource> indexBuffer;
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
        D3D12_INDEX_BUFFER_VIEW indexBufferView;
    };

    struct Model
    {
        Mesh mesh;
    };

    struct MVPTransforms
    {
        DirectX::XMFLOAT4X4 model;
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 projection;
        DirectX::XMFLOAT4X4 viewProjection;
    };

    struct Camera
    {
        DirectX::XMVECTOR position;
        DirectX::XMVECTOR direction = { 0.0f, 0.0f, -1.0f, 0.0f };
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 projection;

        void updateView();
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


        void onUpdate();
        void onRender();
        void onResize(uint16_t width, uint16_t height);

        inline std::wstring GetAssetFullPath(LPCWSTR assetFileName)
        {
            return m_renderConfig.assetsPath + assetFileName;
        };

        RenderConfig m_renderConfig;
        HWND m_windowHandle = nullptr;

    private:
        static constexpr uint32_t FRAME_COUNT = 2u;

        void recreateRenderTargetViews();
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
        ComPtr<ID3D12Resource> m_depthVuffer;
        ComPtr<ID3D12DescriptorHeap> m_DSVHeap;
        ComPtr<ID3D12RootSignature> m_rootSignature;
        ComPtr<ID3D12PipelineState> m_pipelineState;

        ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
        ComPtr<ID3D12Resource> m_constantBuffer;
        MVPTransforms m_mvpTransforms;
        uint8_t* m_pCbvDataBegin;

        ComPtr<ID3D12Fence> m_fence;
        HANDLE m_fenceEvent;
        uint64_t m_fenceValues[FRAME_COUNT];
        uint32_t m_frameIndex = 0;

        Model m_cube;

        uint32_t m_rtvDescriptorSize = 0;

        float m_aspectRatio;
        bool m_contentLoaded = false;
    };
}