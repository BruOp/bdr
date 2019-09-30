#include "renderer.h"
#include "dx_helpers.h"

namespace bdr
{
    Renderer::Renderer(const RenderConfig& renderConfig) :
        m_renderConfig(renderConfig),
        m_rtvDescriptorSize{ 0 },
        m_frameIndex{ 0 },
        m_aspectRatio{ static_cast<float>(renderConfig.width) / static_cast<float>(renderConfig.height) },
        m_viewport{ 0.0f, 0.0f, static_cast<FLOAT>(renderConfig.width), static_cast<FLOAT>(renderConfig.height) },
        m_scissorRect{ 0, 0, static_cast<LONG>(renderConfig.width), static_cast<LONG>(renderConfig.height) }
    { }


    Renderer::~Renderer()
    {
        OutputDebugString(L"Cleaning up renderer\n");
    }


    void Renderer::init(HWND windowHandle)
    {
        OutputDebugString(L"Initializing Renderer!\n");
        m_windowHandle = windowHandle;
        initPipeline();
        initAssets();
    }

    void Renderer::initPipeline()
    {

        UINT dxgiFactoryFlags = 0;

    #if defined(_DEBUG)
        // Enable the debug layer
        {
            ComPtr<ID3D12Debug> debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
                debugController->EnableDebugLayer();

                // Enable additional debug layers.
                dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            }
        }
    #endif

        ComPtr<IDXGIFactory4> factory;
        ThrowIfFailed(CreateDXGIFactory2(
            dxgiFactoryFlags,
            IID_PPV_ARGS(&factory)
        ));

        // Get our device
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        getHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(&m_device)
        ));

        // Graphics Command Queue
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_graphicsQueue)));

        // Swap Chain
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.BufferCount = FRAME_COUNT;
        swapChainDesc.Width = uint32_t{ m_renderConfig.width };
        swapChainDesc.Height = uint32_t{ m_renderConfig.height };
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.SampleDesc.Count = 1;

        ComPtr<IDXGISwapChain1> swapChain;
        ThrowIfFailed(factory->CreateSwapChainForHwnd(
            m_graphicsQueue.Get(),
            m_windowHandle,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain
        ));

        // TODO: Support Window Fullscreen
        ThrowIfFailed(factory->MakeWindowAssociation(
            m_windowHandle,
            DXGI_MWA_NO_ALT_ENTER
        ));

        ThrowIfFailed(swapChain.As(&m_swapChain));
        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

        // Descriptor Heaps
        {
            // Render Target View (rtv) heap
            D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
            rtvHeapDesc.NumDescriptors = FRAME_COUNT;
            rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            ThrowIfFailed(m_device->CreateDescriptorHeap(
                &rtvHeapDesc,
                IID_PPV_ARGS(&m_rtvHeap)
            ));

            m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        }

        // Frame resources
        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

            // Create an RTV handle for each frame
            for (uint32_t n = 0; n < FRAME_COUNT; n++) {
                ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
                m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
                rtvHandle.Offset(1, m_rtvDescriptorSize);
            }
        }

        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
    }


    void Renderer::initAssets()
    {
        // TODO: What is a root signature??
        {
            CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
            rootSignatureDesc.Init(
                0,
                nullptr,
                0,
                nullptr,
                D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
            );

            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
            ThrowIfFailed(m_device->CreateRootSignature(
                0,
                signature->GetBufferPointer(),
                signature->GetBufferSize(),
                IID_PPV_ARGS(&m_rootSignature)
            ));
        }

        //PSO and Shaders
        {
            ComPtr<ID3DBlob> vertexShader;
            ComPtr<ID3DBlob> pixelShader;

        #if defined(_DEBUG)
            UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
        #else
            UINT compileFlags = 0;
        #endif

            // Compile our fragment and pixel shader
            ThrowIfFailed(D3DCompileFromFile(
                GetAssetFullPath(L"shaders.hlsl").c_str(),
                nullptr,
                nullptr,
                "VSMain",
                "vs_5_0",
                compileFlags,
                0,
                &vertexShader,
                nullptr
            ));

            ThrowIfFailed(D3DCompileFromFile(
                GetAssetFullPath(L"shaders.hlsl").c_str(),
                nullptr,
                nullptr,
                "PSMain",
                "ps_5_0",
                compileFlags,
                0,
                &pixelShader,
                nullptr
            ));

            // Vertex Input Layout
            D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            };

            // Pipeline State Object
            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
            psoDesc.pRootSignature = m_rootSignature.Get();
            psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
            psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
            psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
            psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
            psoDesc.DepthStencilState.DepthEnable = FALSE;
            psoDesc.DepthStencilState.StencilEnable = FALSE;
            psoDesc.SampleMask = UINT_MAX;
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            psoDesc.NumRenderTargets = 1;
            psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
            psoDesc.SampleDesc.Count = 1;

            ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
        }

        // Command List
        ThrowIfFailed(m_device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_commandAllocator.Get(),
            m_pipelineState.Get(),
            IID_PPV_ARGS(&m_commandList)
        ));

        // CommandLists are created in the recording state, but we're not recording yet.
        // Let's close it!
        ThrowIfFailed(m_commandList->Close());

        // Vertex Buffer
        {
            Vertex triangleVerts[] = {
                { { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 0xFF0000FF } },
                { { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0x00FF00FF } },
                { { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0x0000FFFF } },
            };

            const UINT vertexBufferSize = sizeof(triangleVerts);

            // Note: using upload heaps to transfer static data like vert buffers is not 
            // recommended. Every time the GPU needs it, the upload heap will be marshalled 
            // over. Please read up on Default Heap usage. An upload heap is used here for 
            // code simplicity and because there are very few verts to actually transfer.
            ThrowIfFailed(m_device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_vertexBuffer)
            ));

            // Copy
            uint8_t* pVertexDataBegin = nullptr;
            // This is zero because: " We do not intend to read from this resource on the CPU."
            CD3DX12_RANGE readRange(0, 0);
            ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
            memcpy(pVertexDataBegin, triangleVerts, vertexBufferSize);
            m_vertexBuffer->Unmap(0, nullptr);

            // Initialize the vertex buffer view
            m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
            m_vertexBufferView.StrideInBytes = sizeof(Vertex);
            m_vertexBufferView.SizeInBytes = vertexBufferSize;
        }

        // Create fence for frame synchronization
        {
            ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
            m_fenceValue = 1;

            m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (m_fenceEvent == nullptr) {
                ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
            }

            // Wait for our setup to complete before continuing
            waitForPreviousFrame();
        }
    }

    void Renderer::onRender()
    {
        // Record all the commands we need to render the scene into the command list
        populateCommandList();

        // Execute the command list
        ID3D12CommandList* ppCommandLists[]{ m_commandList.Get() };
        m_graphicsQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        // Present
        // TODO: Look up syncInterval parameter here
        ThrowIfFailed(m_swapChain->Present(1, 0));

        waitForPreviousFrame();
    }

    void Renderer::populateCommandList()
    {
        // Command list allocators can only be reset when the associated 
        // command lists have finished execution on the GPU; apps should use 
        // fences to determine GPU execution progress.
        ThrowIfFailed(m_commandAllocator->Reset());

        // However, when ExecuteCommandList() is called on a particular command 
        // list, that command list can then be reset at any time and must be before 
        // re-recording.
        ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

        // Set necessary state.
        m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);

        // Indicate that the back buffer will be used as a render target.
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
        m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        // Record commands.
        const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        m_commandList->DrawInstanced(3, 1, 0, 0);

        // Indicate that the back buffer will now be used to present.
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

        ThrowIfFailed(m_commandList->Close());

    }

    void Renderer::waitForPreviousFrame()
    {
        const uint64_t fence = m_fenceValue;
        ThrowIfFailed(m_graphicsQueue->Signal(m_fence.Get(), fence));
        ++m_fenceValue;

        if (m_fence->GetCompletedValue() < fence) {
            ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }

        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    }


    _Use_decl_annotations_
        void Renderer::getHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter)
    {
        ComPtr<IDXGIAdapter1> adapter;
        *ppAdapter = nullptr;

        for (uint32_t adapterIndex = 0; pFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; ++adapterIndex) {
            DXGI_ADAPTER_DESC1 adapterDesc;
            adapter->GetDesc1(&adapterDesc);

            if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                // We want to avoid grabbing the software adapter, even if it is found
                continue;
            }

            // For each adapter we find, that's not a software adapter, we try to create a device. Once we are successful, break!
            if (SUCCEEDED(
                D3D12CreateDevice(
                    adapter.Get(),
                    D3D_FEATURE_LEVEL_12_0,
                    __uuidof(ID3D12Device),
                    nullptr
                )
            )) {
                break;
            }
        }
        // Store our new adapter in the IDXGIAdapter function parameter
        *ppAdapter = adapter.Detach();
    }
}