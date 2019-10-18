#include "renderer.h"
#include "dx_helpers.h"

using namespace DirectX;

namespace bdr
{
    Renderer::Renderer(const RenderConfig& renderConfig) :
        m_renderConfig(renderConfig),
        m_rtvDescriptorSize{ 0 },
        m_frameIndex{ 0 },
        m_aspectRatio{ static_cast<float>(renderConfig.width) / static_cast<float>(renderConfig.height) },
        m_viewport{ 0.0f, 0.0f, static_cast<FLOAT>(renderConfig.width), static_cast<FLOAT>(renderConfig.height) },
        m_scissorRect{ 0, 0, LONG_MAX, LONG_MAX }
    { }


    Renderer::~Renderer()
    {
        OutputDebugString(L"Cleaning up renderer\n");

        waitForGPU();

        m_cube.destroy();

        for (size_t i = 0; i < FRAME_COUNT; i++) {
            m_renderTargets[i].destroy();
            m_depthBuffers[i].destroy();
        }
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
        ASSERT_SUCCEEDED(CreateDXGIFactory2(
            dxgiFactoryFlags,
            IID_PPV_ARGS(&factory)
        ));

        // Get our device
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        getHardwareAdapter(factory.Get(), &hardwareAdapter);

        ASSERT_SUCCEEDED(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(&m_device)
        ));

        m_cmdQueueManager.init(m_device.Get());

        m_gpuBufferManager.init(m_device.Get(), &m_cmdQueueManager);

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
            m_cmdQueueManager.m_graphicsQueue.get(),
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
            ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

            m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

            D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
            dsvHeapDesc.NumDescriptors = FRAME_COUNT;
            dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

            // CBV Heaps
            D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
            cbvHeapDesc.NumDescriptors = 1;
            cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            ThrowIfFailed(m_device->CreateDescriptorHeap(
                &cbvHeapDesc,
                IID_PPV_ARGS(&m_cbvHeap)
            ));
        }
        // Initialize our render target views
        recreateRenderTargetViews();

        // Create command allocators for each frame
        for (uint32_t i = 0; i < FRAME_COUNT; ++i) {
            ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])));
        }
    }


    void Renderer::initAssets()
    {
        // TODO: What is a root signature??
        {
            D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

            if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
                featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
            }

            CD3DX12_DESCRIPTOR_RANGE1 ranges[1]{};
            CD3DX12_ROOT_PARAMETER1 rootParameters[1]{};

            ranges[0].Init(
                D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                1,
                0,
                0,
                D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC
            );
            rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);

            D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
            rootSignatureDesc.Init_1_1(
                _countof(rootParameters),
                rootParameters,
                0,
                nullptr,
                flags
            );

            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
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

            ComPtr<ID3DBlob> error;
            // Compile our fragment and pixel shader
            HRESULT hr = D3DCompileFromFile(
                GetAssetFullPath(L"shaders.hlsl").c_str(),
                nullptr,
                nullptr,
                "VSMain",
                "vs_5_1",
                compileFlags,
                0,
                &vertexShader,
                &error
            );

            if (FAILED(hr)) {
                if (error.Get() != nullptr) {
                    OutputDebugStringA((char*)error->GetBufferPointer());
                }
                ThrowIfFailed(hr);
            }

            ThrowIfFailed(D3DCompileFromFile(
                GetAssetFullPath(L"shaders.hlsl").c_str(),
                nullptr,
                nullptr,
                "PSMain",
                "ps_5_1",
                compileFlags,
                0,
                &pixelShader,
                nullptr
            ));

            // Vertex Input Layout
            D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            };

            // Pipeline State Object
            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
            psoDesc.pRootSignature = m_rootSignature.Get();
            psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
            psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
            psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
            psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
            psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
            /*psoDesc.DepthStencilState.DepthEnable = TRUE;
            psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;*/
            psoDesc.DepthStencilState.StencilEnable = FALSE;
            psoDesc.SampleMask = UINT_MAX;
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            psoDesc.NumRenderTargets = 1;
            psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
            psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
            psoDesc.SampleDesc.Count = 1;

            ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
        }

        // Command List
        ThrowIfFailed(m_device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_commandAllocators[m_frameIndex].Get(),
            m_pipelineState.Get(),
            IID_PPV_ARGS(&m_commandList)
        ));

        // CommandLists are created in the recording state, but we're not recording yet.
        // Let's close it!
        ThrowIfFailed(m_commandList->Close());

        // Vertex Buffer
        {
            const UINT vertexBufferSize = sizeof(cubeVertices);
            const UINT indexBufferSize = sizeof(cubeIndices);

            m_cube.mesh.vertexBuffer = m_gpuBufferManager.createOnGPU(
                L"vertex buffer",
                _countof(cubeVertices),
                sizeof(Vertex),
                cubeVertices
            );

            // Initialize the vertex buffer view... this should maybe be done as part of the above
            m_cube.mesh.vertexBufferView.BufferLocation = m_cube.mesh.vertexBuffer.resource->GetGPUVirtualAddress();
            m_cube.mesh.vertexBufferView.StrideInBytes = sizeof(Vertex);
            m_cube.mesh.vertexBufferView.SizeInBytes = vertexBufferSize;

            m_cube.mesh.indexBuffer = m_gpuBufferManager.createOnGPU(
                L"index buffer",
                _countof(cubeIndices),
                sizeof(uint16_t),
                cubeIndices
            );

            // Initialize the vertex buffer view
            m_cube.mesh.indexBufferView.BufferLocation = m_cube.mesh.indexBuffer.resource->GetGPUVirtualAddress();
            m_cube.mesh.indexBufferView.SizeInBytes = indexBufferSize;
            m_cube.mesh.indexBufferView.Format = DXGI_FORMAT_R16_UINT;
        }

        // Constant Buffer
        {
            // TODO: What is the deal with the third argument
            ThrowIfFailed(m_device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_constantBuffer)
            ));

            // Describe and create the constant buffer view
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{ };
            cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = (sizeof(MVPTransforms) + 255) & ~255; // CB size is required to be 256-byte aligned
            m_device->CreateConstantBufferView(&cbvDesc, m_cbvHeap->GetCPUDescriptorHandleForHeapStart());

            // Map and initialize the constant buffer. Since we update this each frame, we won't unmap until we close the application
            // e.g. the full lifetime of the resource
            CD3DX12_RANGE readRange(0, 0);
            ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin)));
            memcpy(m_pCbvDataBegin, &m_mvpTransforms, sizeof(m_mvpTransforms));
        }

        // Wait for our setup to complete before continuing
        waitForGPU();
    }

    void Renderer::onUpdate()
    {
        const float translationSpeed = 0.005f;
        const float offsetBounds = 1.25f;

        XMStoreFloat4x4(&m_mvpTransforms.model, XMMatrixIdentity());

        XMMATRIX view = XMMatrixLookAtRH(XMVECTOR{ 0.0f, 0.0f, 2.0f, 0.0f }, XMVECTOR{ 0.0f, 0.0f, 0.0f, 0.0f }, XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f });
        XMStoreFloat4x4(&m_mvpTransforms.view, view);

        XMMATRIX projection = XMMatrixPerspectiveFovRH(XMConvertToRadians(60.0f), m_aspectRatio, 0.1f, 100.0f);
        XMStoreFloat4x4(&m_mvpTransforms.projection, projection);

        XMStoreFloat4x4(&m_mvpTransforms.viewProjection, XMMatrixMultiply(view, projection));

        memcpy(m_pCbvDataBegin, &m_mvpTransforms, sizeof(m_mvpTransforms));
    }

    void Renderer::onRender()
    {
        // Record all the commands we need to render the scene into the command list
        populateCommandList();

        // Execute the command list
        m_fenceValues[m_frameIndex] = m_cmdQueueManager.m_graphicsQueue.executeCommandList(m_commandList.Get());

        // Present
        // TODO: Look up syncInterval parameter here
        ThrowIfFailed(m_swapChain->Present(1, 0));

        moveToNextFrame();
    }

    void Renderer::onResize(uint16_t width, uint16_t height)
    {
        if (width != m_renderConfig.width || height != m_renderConfig.height) {
            width = XMMax<uint16_t>(1u, width);
            height = XMMax<uint16_t>(1u, height);

            m_renderConfig.width = width;
            m_renderConfig.height = height;

            m_viewport.Width = width;
            m_viewport.Height = height;
            m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);

            // Release existing references to the backbuffers and swapchain
            for (uint32_t i = 0; i < FRAME_COUNT; ++i) {
                // TODO: Make reset part of the resource interface?
                m_renderTargets[i].destroy();
                m_depthBuffers[i].destroy();
            }

            waitForGPU();

            DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
            ThrowIfFailed(m_swapChain->GetDesc(&swapChainDesc));
            ThrowIfFailed(m_swapChain->ResizeBuffers(FRAME_COUNT, width, height,
                swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

            m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

            recreateRenderTargetViews();
        }
    }

    void Renderer::recreateRenderTargetViews()
    {

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create an RTV handle for each frame
        for (uint32_t n = 0; n < FRAME_COUNT; n++) {
            ID3D12Resource** ppRenderTarget = m_renderTargets[n].getPPtr();
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(ppRenderTarget)));
            m_device->CreateRenderTargetView(*ppRenderTarget, nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);
            (*ppRenderTarget)->SetName(L"Render Target");

            // Resize screen dependent resources.
            // Create a depth buffer.
            D3D12_CLEAR_VALUE optimizedClearValue = {};
            optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
            optimizedClearValue.DepthStencil = { 1.0f, 0 };

            ID3D12Resource** ppDepthBuffer = m_depthBuffers[n].getPPtr();
            ThrowIfFailed(m_device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Tex2D(
                    DXGI_FORMAT_D32_FLOAT,
                    m_renderConfig.width,
                    m_renderConfig.height,
                    1, 0, 1, 0,
                    D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
                ),
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &optimizedClearValue,
                IID_PPV_ARGS(ppDepthBuffer)
            ));
            (*ppDepthBuffer)->SetName(L"Depth Buffer");

            // Update the depth-stencil view.
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;
            dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

            m_device->CreateDepthStencilView(*ppDepthBuffer, &dsvDesc, dsvHandle);
            dsvHandle.Offset(1, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
        }
    }

    void Renderer::populateCommandList()
    {
        //
        // Command list allocators can only be reset when the associated 
        // command lists have finished execution on the GPU; apps should use 
        // fences to determine GPU execution progress.
        ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());

        // However, when ExecuteCommandList() is called on a particular command 
        // list, that command list can then be reset at any time and must be before 
        // re-recording.
        ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));

        m_gpuBufferManager.execute(true);
        // TODO: Figure out a better way to manage this thing's state
        m_gpuBufferManager.reset();

        // Set necessary state.
        m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

        ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get() };
        m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
        m_commandList->SetGraphicsRootDescriptorTable(0, m_cbvHeap->GetGPUDescriptorHandleForHeapStart());

        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);

        // Indicate that the back buffer will be used as a render target.
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(
            m_dsvHeap->GetCPUDescriptorHandleForHeapStart(),
            m_frameIndex,
            m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
        );

        m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

        // Record commands.
        const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->IASetVertexBuffers(0, 1, &m_cube.mesh.vertexBufferView);
        m_commandList->IASetIndexBuffer(&m_cube.mesh.indexBufferView);
        m_commandList->DrawIndexedInstanced(_countof(cubeIndices), 1, 0, 0, 0);

        // Indicate that the back buffer will now be used to present.
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

        ThrowIfFailed(m_commandList->Close());

    }

    void Renderer::waitForGPU()
    {
        m_cmdQueueManager.m_graphicsQueue.waitForIdle();
    }

    void Renderer::moveToNextFrame()
    {
        const uint64_t fenceValue = m_fenceValues[m_frameIndex];

        // Update the frame index.
        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

        // If the next frame is not ready to be rendered yet, wait until it is ready.
        m_cmdQueueManager.m_graphicsQueue.waitForFence(fenceValue);

        //// Set the fence value for the next frame.
        //m_fenceValues[m_frameIndex] = fenceValue + 1;
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


    void Camera::updateView()
    {
        XMVECTOR up = XMVectorGetY(direction) != 1.0f ? XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f } : XMVECTOR{ 1.0f, 0.0f, 0.0f, 0.0f };

        XMMatrixLookAtRH(position, XMVectorAdd(position, direction), up);
    };
}