// SleepyEngine.cpp : Defines the entry point for the application.
//
//#include "pch.h"
#include "SleepyEngine.h"
#include "Utils/HResultException.h"
#include "PSO.h"
#include "Shader.h"
#include <comdef.h>
#include <iostream>
#include <array>
#include "Mesh.h"


// Forward declarations of functions included in this code module:
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


SleepyEngine::SleepyEngine(HINSTANCE hInstance)
{
    m_hAppInstance = hInstance;
}

void SleepyEngine::InitD3D()
{
#if defined(DEBUG) || defined(_DEBUG)
    EnableAdditionalD3D12Debug();
#endif
    CreateDevice();
    CreateFence();
    RecoverDescriptorsSize();
    Check4xMSAAQualitySupport();
    CreateCommandObjects();
    CreateSwapChain();
    CreateDescriptorHeaps();
    CreateRenderTargetView();
    ThrowIfFailed(m_pCommandList->Reset(m_pDirectCmdListAlloc, nullptr));
    CreateDepthStencilView();
    SetViewport();
    SetScissorRect();
    BuildDescriptorHeaps();
    BuildConstantBuffers();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildBoxGeometry();
    BuildPSO();
    // The window resized, so update the aspect ratio and recompute the projection matrix.
    DirectX::XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, static_cast<float>(m_clientWidth/m_clientHeight), 1.0f, 1000.0f);
    DirectX::XMStoreFloat4x4(&mProj, P);
    
}
void SleepyEngine::EnableAdditionalD3D12Debug()
{
    ID3D12Debug* pDebugController;
    ThrowIfFailed(D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void**)&pDebugController)); //should throw error
    pDebugController->EnableDebugLayer();
    std::cout << "Debug Layer Enabled" << std::endl;
}

void SleepyEngine::CreateDevice()
{
    ThrowIfFailed(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&m_pDxgiFactory));
    ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), (void**)&m_pDevice));
    //if failed, check book for "WARP_Adapters".
}

void SleepyEngine::CreateFence()
{
    ThrowIfFailed(m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&m_pFence));
}

void SleepyEngine::RecoverDescriptorsSize()
{
    m_rtvDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_dsvDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    m_cbvSrvUavDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void SleepyEngine::Check4xMSAAQualitySupport()
{
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels; //will likely be moved to private member attribute
    msQualityLevels.Format = m_backBufferFormat;
    msQualityLevels.SampleCount = 4;
    msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    msQualityLevels.NumQualityLevels = 0;
    ThrowIfFailed(m_pDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels, sizeof(msQualityLevels)));
    m_4xMsaaQuality = msQualityLevels.NumQualityLevels;
    assert(m_4xMsaaQuality > 0 && "Unexpected MSAA quality level.");
}

void SleepyEngine::CreateCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ThrowIfFailed(m_pDevice->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void**)&m_pCommandQueue));
    ThrowIfFailed(m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&m_pDirectCmdListAlloc));
    ThrowIfFailed(m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pDirectCmdListAlloc, nullptr, __uuidof(ID3D12CommandList), (void**)&m_pCommandList));
    
    // Start off in a closed state. This is because the first time we 
    // refer to the command list we will Reset it, and it needs to be 
    // closed before calling Reset.
    
    // Seems like we need the commandlist opened before we reset it 
    // again for the ressource barrier and viewport inits

    m_pCommandList->Close(); 
}

void SleepyEngine::CreateSwapChain()
{
    delete m_pSwapChain;
    DXGI_SWAP_CHAIN_DESC swapChainDescriptor;
    swapChainDescriptor.BufferDesc.Width = m_clientWidth;
    swapChainDescriptor.BufferDesc.Height = m_clientHeight;
    swapChainDescriptor.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDescriptor.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDescriptor.BufferDesc.Format = m_backBufferFormat;
    swapChainDescriptor.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDescriptor.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDescriptor.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
    swapChainDescriptor.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
    swapChainDescriptor.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDescriptor.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
    swapChainDescriptor.OutputWindow = mhMainWnd;
    swapChainDescriptor.Windowed = true;
    swapChainDescriptor.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDescriptor.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    ThrowIfFailed(m_pDxgiFactory->CreateSwapChain(m_pCommandQueue, &swapChainDescriptor, &m_pSwapChain)); //CreateSwapChainForHwnd()
}

void SleepyEngine::CreateDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = SWAP_CHAIN_BUFFER_COUNT;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    ThrowIfFailed(m_pDevice->CreateDescriptorHeap(&rtvHeapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&m_pRtvHeap));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(m_pDevice->CreateDescriptorHeap(&dsvHeapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&m_pDsvHeap));
}

void SleepyEngine::CreateRenderTargetView()
{
    //ID3D12Resource* m_pSwapChainBuffer[SWAP_CHAIN_BUFFER_COUNT]; // must be freed when finished
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_pRtvHeap->GetCPUDescriptorHandleForHeapStart());

    for (UINT i = 0; i < SWAP_CHAIN_BUFFER_COUNT; i++)
    {
        ThrowIfFailed(m_pSwapChain->GetBuffer(i, __uuidof(ID3D12Resource), (void**)&m_pSwapChainBuffer[i]));
        m_pDevice->CreateRenderTargetView(m_pSwapChainBuffer[i], nullptr, rtvHeapHandle);
        rtvHeapHandle.Offset(1, m_rtvDescriptorSize); 
    }
}

void SleepyEngine::CreateDepthStencilView()
{
    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = m_clientWidth;
    depthStencilDesc.Height = m_clientHeight;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilDesc.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
    depthStencilDesc.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    // Describes a value used to optimize clear operations for a particular resource.
    // https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_clear_value
    D3D12_CLEAR_VALUE optClear;
    optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;

    CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(m_pDevice->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_COMMON,
        &optClear,
        __uuidof(ID3D12Resource),
        (void**)&m_pDepthStencilBuffer
    ));

    /*D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.Texture2D.MipSlice = 0;*/
    m_pDevice->CreateDepthStencilView(m_pDepthStencilBuffer, nullptr, GetDepthStencilView()); //nullptr for default mip level 0

    CD3DX12_RESOURCE_BARRIER ressourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_pDepthStencilBuffer,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_DEPTH_WRITE);
    m_pCommandList->ResourceBarrier(
        1,
        &ressourceBarrier
    );
}

void SleepyEngine::SetViewport()
{
    m_pViewPort->TopLeftX = 0.0f;
    m_pViewPort->TopLeftY = 0.0f;
    m_pViewPort->Width = static_cast<float>(m_clientWidth);
    m_pViewPort->Height = static_cast<float>(m_clientHeight);
    m_pViewPort->MinDepth = 0.0f;
    m_pViewPort->MaxDepth = 1.0f;
    m_pCommandList->RSSetViewports(1, m_pViewPort);
}

// Scissor rect is used to clip pixels to a certain area of the render target and avoid rendering them (ex: pixels below UI).
void SleepyEngine::SetScissorRect()
{
    m_scissorRect.left = 0;
    m_scissorRect.top = 0;
    m_scissorRect.right = m_clientWidth;
    m_scissorRect.bottom = m_clientHeight;
    m_pCommandList->RSSetScissorRects(1, &m_scissorRect);
}

int SleepyEngine::Initialize()
{
    try
    {
        LoadStringW(m_hAppInstance, IDS_APP_TITLE, m_szTitle, MAX_LOADSTRING);
        LoadStringW(m_hAppInstance, IDC_SLEEPYENGINE, m_szWindowClass, MAX_LOADSTRING);
        RegisterWindowClass();
        InitWindow(SW_SHOW);
        InitD3D();
    }
    catch (HResultException error)
    {
        std::wstring messageBoxText;
        MessageBox(nullptr, error.ToString().c_str(), L"HRESULT ERROR", MB_OK);
    	HRESULT hr = m_pDevice->GetDeviceRemovedReason();
        if (FAILED(hr))
        {
            _com_error error(hr);
            std::string errorMessage = "Device Removed Reason:\n" + D3DUtils::HrToString(hr);
            MessageBoxA(nullptr, errorMessage.c_str(), "Device Removed", MB_OK | MB_ICONERROR);
            MessageBox(nullptr, error.ErrorMessage(),L"Device Removed", MB_OK);
        }
        return 1;
    }
    return 0;
}


void SleepyEngine::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(m_pDevice->CreateDescriptorHeap(&cbvHeapDesc,
        IID_PPV_ARGS(&m_pCbvHeap)));
}

void SleepyEngine::BuildConstantBuffers()
{
    m_pObjectCB = new UploadBuffer<ObjectConstants>(m_pDevice, 1, true);

    UINT objCBByteSize = ((sizeof(ObjectConstants) + 255) & ~255);

    D3D12_GPU_VIRTUAL_ADDRESS cbAddress = m_pObjectCB->Resource()->GetGPUVirtualAddress();
    // Offset to the ith object constant buffer in the buffer.
    int boxCBufIndex = 0;
    cbAddress += boxCBufIndex * objCBByteSize;

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    cbvDesc.BufferLocation = cbAddress;
    cbvDesc.SizeInBytes = ((sizeof(ObjectConstants) + 255) & ~255);

    m_pDevice->CreateConstantBufferView(
        &cbvDesc,
        m_pCbvHeap->GetCPUDescriptorHandleForHeapStart());

}

void SleepyEngine::BuildRootSignature()
{
    // Shader programs typically require resources as input (constant buffers,
    // textures, samplers).  The root signature defines the resources the shader
    // programs expect.  If we think of the shader programs as a function, and
    // the input resources as function parameters, then the root signature can be
    // thought of as defining the function signature.  

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];

    // Create a single descriptor table of CBVs.
    CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ID3DBlob* serializedRootSig = nullptr;
    ID3DBlob* errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        &serializedRootSig, &errorBlob);

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(m_pDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&m_pRootSignature)));
}

void SleepyEngine::BuildShadersAndInputLayout()
{
    HRESULT hr = S_OK;

    m_pVSByteCode = D3DUtils::CompileShader(L"C:\\Users\\gdalmon\\source\\repos\\yoannklt\\Sleepy\\SleepyEngine\\src\\shaders\\shader.hlsl", nullptr, "VS", "vs_5_0");
    m_pPSByteCode = D3DUtils::CompileShader(L"C:\\Users\\gdalmon\\source\\repos\\yoannklt\\Sleepy\\SleepyEngine\\src\\shaders\\shader.hlsl", nullptr, "PS", "ps_5_0");

    m_inputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void SleepyEngine::BuildPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    psoDesc.pRootSignature = m_pRootSignature;
    psoDesc.VS =
    {
        reinterpret_cast<BYTE*>(m_pVSByteCode->GetBufferPointer()),
        m_pVSByteCode->GetBufferSize()
    };
    psoDesc.PS =
    {
        reinterpret_cast<BYTE*>(m_pPSByteCode->GetBufferPointer()),
        m_pPSByteCode->GetBufferSize()
    };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = m_backBufferFormat;
    psoDesc.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = m_depthStencilFormat;
    ThrowIfFailed(m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO)));
}

void SleepyEngine::BuildBoxGeometry()
{
    std::array<float, 5> testvar = { 1.f,1.f ,1.f ,1.f ,1.f };
    std::array<Vertex, 8> vertices =
    {
        Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
        Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
        Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
        Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
        Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
        Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
        Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
        Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
    };

    std::array<std::uint16_t, 36> indices =
    {
        // front face
        0, 1, 2,
        0, 2, 3,

        // back face
        4, 6, 5,
        4, 7, 6,

        // left face
        4, 5, 1,
        4, 1, 0,

        // right face
        3, 2, 6,
        3, 6, 7,

        // top face
        1, 5, 6,
        1, 6, 2,

        // bottom face
        4, 0, 3,
        4, 3, 7
    };

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    mBoxGeo = std::make_unique<MeshGeometry>();
    mBoxGeo->Name = "boxGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &mBoxGeo->VertexBufferCPU));
    CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU));
    CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    mBoxGeo->VertexBufferGPU = D3DUtils::CreateDefaultBufferWRL(m_pDevice,
        m_pCommandList, vertices.data(), vbByteSize, mBoxGeo->VertexBufferUploader);

    mBoxGeo->IndexBufferGPU = D3DUtils::CreateDefaultBufferWRL(m_pDevice,
        m_pCommandList, indices.data(), ibByteSize, mBoxGeo->IndexBufferUploader);

    mBoxGeo->VertexByteStride = sizeof(Vertex);
    mBoxGeo->VertexBufferByteSize = vbByteSize;
    mBoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
    mBoxGeo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    mBoxGeo->DrawArgs["box"] = submesh;
}

int SleepyEngine::Run()
{
    HACCEL hAccelTable = LoadAccelerators(m_hAppInstance, MAKEINTRESOURCE(IDC_SLEEPYENGINE));    

    ThrowIfFailed(m_pCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { m_pCommandList };
    m_pCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    FlushCommandQueue();
    MSG msg = { 0 };
    // Main message loop:
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            //Draw();
            Update();
            Draw();
        }
        
    }
    return (int)msg.wParam;
}

void SleepyEngine::Update()
{
    // Convert Spherical to Cartesian coordinates.
    float x = mRadius * sinf(mPhi) * cosf(mTheta);
    float z = mRadius * sinf(mPhi) * sinf(mTheta);
    float y = mRadius * cosf(mPhi);

    // Build the view matrix.
    DirectX::XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
    DirectX::XMVECTOR target = XMVectorZero();
    DirectX::XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    DirectX::XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    DirectX::XMStoreFloat4x4(&mView, view);

    DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&mWorld);
    DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&mProj);
    DirectX::XMMATRIX worldViewProj = world * view * proj;

    // Update the constant buffer with the latest worldViewProj matrix.
    ObjectConstants objConstants;
    DirectX::XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
    m_pObjectCB->CopyData(0, objConstants);
}

D3D12_CPU_DESCRIPTOR_HANDLE SleepyEngine::GetCurrentBackBufferView()const
{
    // CD3DX12 constructor to offset to the RTV of the current back buffer.
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_pRtvHeap->GetCPUDescriptorHandleForHeapStart(),// handle start
        m_currentBackBufferOffset, // index to offset
        m_rtvDescriptorSize); // byte size of descriptor
}

ID3D12Resource* SleepyEngine::GetCurrentBackBuffer()const
{
    return m_pSwapChainBuffer[m_currentBackBufferOffset];
}

D3D12_CPU_DESCRIPTOR_HANDLE SleepyEngine::GetDepthStencilView()const
{
    return m_pDsvHeap->GetCPUDescriptorHandleForHeapStart();
}


ATOM SleepyEngine::RegisterWindowClass()
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = m_hAppInstance;
    wcex.hIcon = LoadIcon(m_hAppInstance, MAKEINTRESOURCE(IDI_SLEEPYENGINE));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = 0;
    wcex.lpszClassName = m_szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

void SleepyEngine::InitWindow(int nCmdShow)
{
    mhMainWnd = CreateWindow(m_szWindowClass, m_szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, m_clientWidth, m_clientHeight, 0, 0, m_hAppInstance, 0);

    if (mhMainWnd == NULL)
    {
        MessageBox(0, L"CreateWindow FAILED", 0, 0);
        return;
    }
    ShowWindow(mhMainWnd, nCmdShow);
    UpdateWindow(mhMainWnd);

}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
            /*case IDM_ABOUT:
                DialogBox((HINSTANCE)wParam, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;*/
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // TODO: Add any drawing code that uses hdc here...
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void SleepyEngine::FlushCommandQueue()
{
    // Advance the fence value to mark commands up to this fence point.
    m_currentFence++;

    // Add an instruction to the command queue to set a new fence point.  Because we 
    // are on the GPU timeline, the new fence point won't be set until the GPU finishes
    // processing all the commands prior to this Signal().
    ThrowIfFailed(m_pCommandQueue->Signal(m_pFence, m_currentFence));

    // Wait until the GPU has completed commands up to this fence point.
    if (m_pFence->GetCompletedValue() < m_currentFence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

        // Fire event when GPU hits current fence.  
        ThrowIfFailed(m_pFence->SetEventOnCompletion(m_currentFence, eventHandle));

        // Wait until the GPU hits current fence event is fired.
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

/*void SleepyEngine::Draw()//const GameTimer& gt)
{

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished
    // execution on the GPU.
    ThrowIfFailed(m_pDirectCmdListAlloc->Reset());
    // A command list can be reset after it has been added to the 
    // command queue via ExecuteCommandList. Reusing the command list 
    // reuses memory.
    ThrowIfFailed(m_pCommandList->Reset(m_pDirectCmdListAlloc, nullptr));

    CD3DX12_RESOURCE_BARRIER resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        GetCurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );

    // Indicate a state transition on the resource usage.
    m_pCommandList->ResourceBarrier(1, &resourceBarrier);
    // Set the viewport and scissor rect. This needs to be reset 
    // whenever the command list is reset.
    m_pCommandList->RSSetViewports(1, m_pViewPort);
    m_pCommandList->RSSetScissorRects(1, &m_scissorRect);
    // Clear the back buffer and depth buffer.
    m_pCommandList->ClearRenderTargetView(
        GetCurrentBackBufferView(),
        DirectX::Colors::DarkRed, 0, nullptr
    );

    m_pCommandList->ClearDepthStencilView(
        GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH |
        D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr
    );

    // Specify the buffers we are going to render to.
    D3D12_CPU_DESCRIPTOR_HANDLE currentBackBufferView = GetCurrentBackBufferView();
    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = GetDepthStencilView();
    m_pCommandList->OMSetRenderTargets(1, &currentBackBufferView, true, &depthStencilView);
    // Indicate a state transition on the resource usage.
    CD3DX12_RESOURCE_BARRIER resourceBarrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
        GetCurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    m_pCommandList->ResourceBarrier(1, &resourceBarrier2);
    // Done recording commands.
    ThrowIfFailed(m_pCommandList->Close());
    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { m_pCommandList };
    m_pCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    // swap the back and front buffers
    ThrowIfFailed(m_pSwapChain->Present(0, 0));
    m_currentBackBufferOffset = (m_currentBackBufferOffset + 1) % SWAP_CHAIN_BUFFER_COUNT;
    // Wait until frame commands are complete. This waiting is 
    // inefficient and is done for simplicity. Later we will show how to 
    // organize our rendering code so we do not have to wait per frame.
    FlushCommandQueue();
}*/

//void SleepyEngine::Draw(ID3D12DescriptorHeap* pCBVHeap, ID3D12RootSignature* pRootSignature, Mesh* mesh)
void SleepyEngine::Draw()
{
    
    std::cout << "Drawing" << std::endl;
    CD3DX12_RESOURCE_BARRIER barrier;

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE currentBackBufferView = GetCurrentBackBufferView();
    D3D12_CPU_DESCRIPTOR_HANDLE dephtStencilView = GetDepthStencilView();

    m_pDirectCmdListAlloc->Reset();

    m_pCommandList->Reset(m_pDirectCmdListAlloc, m_PSO);
    m_pCommandList->RSSetViewports(1, m_pViewPort);
    m_pCommandList->RSSetScissorRects(1, &m_scissorRect);

    // bcareful, may have problems using a single initialization of barrier
    m_pCommandList->ResourceBarrier(1, &barrier);

    m_pCommandList->ClearRenderTargetView(currentBackBufferView, Colors::LightSteelBlue, 0, nullptr);
    m_pCommandList->ClearDepthStencilView(dephtStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    m_pCommandList->OMSetRenderTargets(1, &currentBackBufferView, true, &dephtStencilView);

    ID3D12DescriptorHeap* descriptorHeaps[] = { m_pCbvHeap };
    m_pCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    m_pCommandList->SetGraphicsRootSignature(m_pRootSignature);

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = mBoxGeo->VertexBufferView();//mesh->VertexBufferView();
    D3D12_INDEX_BUFFER_VIEW indexBufferView = mBoxGeo->IndexBufferView();// mesh->IndexBufferView();

    m_pCommandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    m_pCommandList->IASetIndexBuffer(&indexBufferView);

    m_pCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_pCommandList->SetGraphicsRootDescriptorTable(0, m_pCbvHeap->GetGPUDescriptorHandleForHeapStart());



    //m_pCommandList->DrawIndexedInstanced(mesh->m_indexCount, 1, 0, 0, 0);
    m_pCommandList->DrawIndexedInstanced(mBoxGeo->DrawArgs["box"].IndexCount, 1, 0, 0, 0);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_pCommandList->ResourceBarrier(1, &barrier);

    m_pCommandList->Close();

    ID3D12CommandList* cmdsLists[] = { m_pCommandList };
    m_pCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(m_pSwapChain->Present(0, 0));
    m_currentBackBufferOffset = (m_currentBackBufferOffset + 1) % SWAP_CHAIN_BUFFER_COUNT;
    FlushCommandQueue();

    // Reuse the memory associated with command recording.
// We can only reset when the associated command lists have finished execution on the GPU.
    /*ThrowIfFailed(m_pDirectCmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(m_pCommandList->Reset(m_pDirectCmdListAlloc, m_PSO));

    m_pCommandList->RSSetViewports(1, m_pViewPort);
    m_pCommandList->RSSetScissorRects(1, &m_scissorRect);

    // Indicate a state transition on the resource usage.
    m_pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clear the back buffer and depth buffer.
    m_pCommandList->ClearRenderTargetView(GetCurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    m_pCommandList->ClearDepthStencilView(GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    m_pCommandList->OMSetRenderTargets(1, &GetCurrentBackBufferView(), true, &GetDepthStencilView());

    ID3D12DescriptorHeap* descriptorHeaps[] = { m_pCbvHeap };
    m_pCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    m_pCommandList->SetGraphicsRootSignature(m_pRootSignature);

    m_pCommandList->IASetVertexBuffers(0, 1, &mBoxGeo->VertexBufferView());
    m_pCommandList->IASetIndexBuffer(&mBoxGeo->IndexBufferView());
    m_pCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_pCommandList->SetGraphicsRootDescriptorTable(0, m_pCbvHeap->GetGPUDescriptorHandleForHeapStart());

    m_pCommandList->DrawIndexedInstanced(
        mBoxGeo->DrawArgs["box"].IndexCount,
        1, 0, 0, 0);

    // Indicate a state transition on the resource usage.
    m_pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetCurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(m_pCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { m_pCommandList };
    m_pCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // swap the back and front buffers
    ThrowIfFailed(m_pSwapChain->Present(0, 0));
    m_currentBackBufferOffset = (m_currentBackBufferOffset + 1) % SWAP_CHAIN_BUFFER_COUNT;

    // Wait until frame commands are complete.  This waiting is inefficient and is
    // done for simplicity.  Later we will show how to organize our rendering code
    // so we do not have to wait per frame.
    FlushCommandQueue();*/
}
