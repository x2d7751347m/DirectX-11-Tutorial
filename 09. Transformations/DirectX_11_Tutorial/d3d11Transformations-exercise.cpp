#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "winmm.lib")

#include <windows.h>
#include <d3d11_4.h>
#include <directxmath.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;
using namespace DirectX;

// Global Declarations
ComPtr<IDXGISwapChain1> g_pSwapChain;
ComPtr<ID3D11Device1> g_pd3dDevice;
ComPtr<ID3D11DeviceContext1> g_pd3dDeviceContext;
ComPtr<ID3D11RenderTargetView> g_pRenderTargetView;
ComPtr<ID3D11Buffer> g_pVertexBuffer;
ComPtr<ID3D11Buffer> g_pIndexBuffer;
ComPtr<ID3D11VertexShader> g_pVertexShader;
ComPtr<ID3D11PixelShader> g_pPixelShader;
ComPtr<ID3D11InputLayout> g_pVertexLayout;
ComPtr<ID3D11DepthStencilView> g_pDepthStencilView;
ComPtr<ID3D11Texture2D> g_pDepthStencilBuffer;
ComPtr<ID3D11Buffer> g_pConstantBuffer;

// Window constants
constexpr LPCTSTR WndClassName = L"DirectXWindow";
constexpr int Width = 800;
constexpr int Height = 600;

// Fullscreen and resize variables
bool g_IsFullscreen = false;
RECT g_WindowRect = {};
UINT g_ResizeWidth = 0;
UINT g_ResizeHeight = 0;
bool g_ResizePending = false;
DWORD g_ResizeLastTime = 0;
constexpr DWORD RESIZE_DELAY_MS = 300;  // 300ms delay for resize
bool g_DeviceRemoved = false;
D3D11_VIEWPORT g_Viewport = {};
bool g_Resizing = false;
float g_CameraRotationAngle = 0.0f;

// Vertex Structure
struct Vertex
{
    XMFLOAT3 Position;
    XMFLOAT4 Color;
};

// Constant buffer structure
struct ConstantBuffer
{
    XMMATRIX mWVP;
};

// Global matrices
XMMATRIX g_World1;
XMMATRIX g_World2;
XMMATRIX g_View;
XMMATRIX g_Projection;

// Function Prototypes
bool InitializeWindow(HINSTANCE hInstance, int nCmdShow);
bool InitializeDirect3D();
void CleanupDirect3D();
bool InitializeScene();
void UpdateScene();
void DrawScene();
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void ResizeDirectXBuffers(HWND hWnd);
void ToggleFullscreen(HWND hWnd);
void HandleResize();
bool RecreateDevice();
bool RecreateRenderTargetView();
bool CreateDepthStencilView(UINT width, UINT height);

// Entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    if (!InitializeWindow(hInstance, nCmdShow))
    {
        MessageBox(nullptr, L"Window Initialization Failed", L"Error", MB_OK);
        return 0;
    }

    if (!InitializeDirect3D())
    {
        MessageBox(nullptr, L"Direct3D Initialization Failed", L"Error", MB_OK);
        return 0;
    }

    if (!InitializeScene())
    {
        MessageBox(nullptr, L"Scene Initialization Failed", L"Error", MB_OK);
        return 0;
    }

    // Main message loop
    MSG msg = { 0 };
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            UpdateScene();
            DrawScene();
        }
    }

    CleanupDirect3D();
    return static_cast<int>(msg.wParam);
}

bool InitializeWindow(HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASSEX wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszClassName = WndClassName;

    if (!RegisterClassEx(&wcex))
        return false;

    RECT rc = { 0, 0, Width, Height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindow(WndClassName, L"DirectX 11 Demo", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
        return false;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return true;
}

bool InitializeDirect3D()
{
    HRESULT hr = S_OK;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);

    D3D_FEATURE_LEVEL featureLevel;

    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevels, numFeatureLevels, D3D11_SDK_VERSION,
        reinterpret_cast<ID3D11Device**>(g_pd3dDevice.GetAddressOf()),
        &featureLevel,
        reinterpret_cast<ID3D11DeviceContext**>(g_pd3dDeviceContext.GetAddressOf()));

    if (FAILED(hr))
        return false;

    ComPtr<IDXGIFactory2> dxgiFactory;
    hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
    if (FAILED(hr))
        return false;

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = Width;
    sd.Height = Height;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    hr = dxgiFactory->CreateSwapChainForHwnd(g_pd3dDevice.Get(), GetActiveWindow(), &sd, nullptr, nullptr, &g_pSwapChain);
    if (FAILED(hr))
        return false;

    ResizeDirectXBuffers(GetActiveWindow());

    // Create constant buffer
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(ConstantBuffer);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pConstantBuffer);
    if (FAILED(hr))
        return false;

    // Initialize matrices
    g_World1 = XMMatrixIdentity();
    g_World2 = XMMatrixIdentity();

    // Initialize the view matrix
    XMVECTOR Eye = XMVectorSet(0.0f, 3.0f, -8.0f, 0.0f);
    XMVECTOR At = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    g_View = XMMatrixLookAtLH(Eye, At, Up);

    // Initialize the projection matrix
    g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, Width / (FLOAT)Height, 0.01f, 100.0f);

    return true;
}

void CleanupDirect3D()
{
    if (g_pd3dDeviceContext) g_pd3dDeviceContext->ClearState();

    if (g_IsFullscreen)
    {
        g_pSwapChain->SetFullscreenState(FALSE, nullptr);
    }
}

bool InitializeScene()
{
    HRESULT hr = S_OK;

    // Compile the vertex shader
    ComPtr<ID3DBlob> pVSBlob;
    hr = D3DCompileFromFile(L"Effects.fx", nullptr, nullptr, "VS", "vs_5_0", 0, 0, &pVSBlob, nullptr);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
        return false;
    }

    // Create the vertex shader
    hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    if (FAILED(hr))
        return false;

    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = ARRAYSIZE(layout);

    // Create the input layout
    hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
        pVSBlob->GetBufferSize(), &g_pVertexLayout);
    if (FAILED(hr))
        return false;

    // Set the input layout
    g_pd3dDeviceContext->IASetInputLayout(g_pVertexLayout.Get());

    // Compile the pixel shader
    ComPtr<ID3DBlob> pPSBlob;
    hr = D3DCompileFromFile(L"Effects.fx", nullptr, nullptr, "PS", "ps_5_0", 0, 0, &pPSBlob, nullptr);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
        return false;
    }

    // Create the pixel shader
    hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader);
    if (FAILED(hr))
        return false;

    // Create vertex buffer
    Vertex vertices[] =
    {
        { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
        { XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
        { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
        { XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
        { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
        { XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
        { XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) },
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(Vertex) * 8;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA InitData = {};
    InitData.pSysMem = vertices;
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
    if (FAILED(hr))
        return false;

    // Set vertex buffer
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    g_pd3dDeviceContext->IASetVertexBuffers(0, 1, g_pVertexBuffer.GetAddressOf(), &stride, &offset);

    // Create index buffer
    WORD indices[] =
    {
        3,1,0,
        2,1,3,

        0,5,4,
        1,5,0,

        3,4,7,
        0,4,3,

        1,6,5,
        2,6,1,

        2,7,6,
        3,7,2,

        6,4,5,
        7,4,6,
    };

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(WORD) * 36;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    InitData.pSysMem = indices;
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pIndexBuffer);
    if (FAILED(hr))
        return false;

    // Set index buffer
    g_pd3dDeviceContext->IASetIndexBuffer(g_pIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

    // Set primitive topology
    g_pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Create the constant buffer
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(ConstantBuffer);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pConstantBuffer);
    if (FAILED(hr))
        return false;

    // Initialize the world matrices
    g_World1 = XMMatrixIdentity();
    g_World2 = XMMatrixIdentity();

    // Initialize the view matrix
    XMVECTOR Eye = XMVectorSet(0.0f, 3.0f, -8.0f, 0.0f);
    XMVECTOR At = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    g_View = XMMatrixLookAtLH(Eye, At, Up);

    // Initialize the projection matrix
    g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, Width / (FLOAT)Height, 0.01f, 100.0f);

    return true;
}

void UpdateScene()
{
    static float t = 0.0f;
    static ULONGLONG dwTimeStart = 0;
    ULONGLONG dwTimeCur = GetTickCount64();
    if (dwTimeStart == 0)
        dwTimeStart = dwTimeCur;
    t = (dwTimeCur - dwTimeStart) / 1000.0f;

    // Exercise 1: Orbit the first cube around a different axis
    // We'll orbit around an axis (1, 1, 1)
    XMVECTOR orbitAxis = XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
    orbitAxis = XMVector3Normalize(orbitAxis);

    XMMATRIX orbit = XMMatrixRotationAxis(orbitAxis, t);
    XMMATRIX translation = XMMatrixTranslation(3.0f, 0.0f, 0.0f);
    XMMATRIX rotation = XMMatrixRotationY(t * 2.0f);
    XMMATRIX scaling = XMMatrixScaling(0.5f + 0.25f * sinf(t), 0.5f + 0.25f * sinf(t), 0.5f + 0.25f * sinf(t));

    // Exercise 2: Play with different transformations and their order
    // Order 1: Orbit -> Translate -> Rotate -> Scale
    g_World1 = scaling * rotation * translation * orbit;

    // Order 2: Scale -> Rotate -> Translate -> Orbit
    // Uncomment the next line and comment out the previous g_World1 assignment to see the difference
    // g_World1 = orbit * translation * rotation * scaling;

    // Update the second cube
    g_World2 = XMMatrixTranslation(4.0f * cosf(t), 2.0f * sinf(t), 0.0f) * XMMatrixRotationZ(t * 3.0f);
}

void DrawScene()
{
    if (!g_pRenderTargetView || !g_pDepthStencilView)
    {
        if (!RecreateRenderTargetView() || !CreateDepthStencilView(Width, Height))
        {
            return;
        }
    }

    float ClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f }; // RGBA
    g_pd3dDeviceContext->ClearRenderTargetView(g_pRenderTargetView.Get(), ClearColor);
    g_pd3dDeviceContext->ClearDepthStencilView(g_pDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    g_pd3dDeviceContext->OMSetRenderTargets(1, g_pRenderTargetView.GetAddressOf(), g_pDepthStencilView.Get());
    g_pd3dDeviceContext->RSSetViewports(1, &g_Viewport);

    // Set shaders
    g_pd3dDeviceContext->VSSetShader(g_pVertexShader.Get(), nullptr, 0);
    g_pd3dDeviceContext->PSSetShader(g_pPixelShader.Get(), nullptr, 0);

    // Draw the first cube
    ConstantBuffer cb1;
    cb1.mWVP = XMMatrixTranspose(g_World1 * g_View * g_Projection);
    g_pd3dDeviceContext->UpdateSubresource(g_pConstantBuffer.Get(), 0, nullptr, &cb1, 0, 0);
    g_pd3dDeviceContext->VSSetConstantBuffers(0, 1, g_pConstantBuffer.GetAddressOf());
    g_pd3dDeviceContext->DrawIndexed(36, 0, 0);

    // Draw the second cube
    ConstantBuffer cb2;
    cb2.mWVP = XMMatrixTranspose(g_World2 * g_View * g_Projection);
    g_pd3dDeviceContext->UpdateSubresource(g_pConstantBuffer.Get(), 0, nullptr, &cb2, 0, 0);
    g_pd3dDeviceContext->VSSetConstantBuffers(0, 1, g_pConstantBuffer.GetAddressOf());
    g_pd3dDeviceContext->DrawIndexed(36, 0, 0);

    HRESULT hr = g_pSwapChain->Present(0, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        RecreateDevice();
    }
    else if (FAILED(hr))
    {
        MessageBox(GetActiveWindow(), L"Failed to present swap chain buffer", L"Error", MB_OK);
    }
}

bool RecreateRenderTargetView()
{
    // Release the old render target view
    g_pRenderTargetView.Reset();

    // Get the texture from the swap chain
    ComPtr<ID3D11Texture2D> pBackBuffer;
    HRESULT hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (FAILED(hr))
    {
        MessageBox(GetActiveWindow(), L"Failed to get swap chain buffer", L"Error", MB_OK);
        return false;
    }

    // Create the render target view
    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &g_pRenderTargetView);
    if (FAILED(hr))
    {
        MessageBox(GetActiveWindow(), L"Failed to create render target view", L"Error", MB_OK);
        return false;
    }

    return true;
}

bool CreateDepthStencilView(UINT width, UINT height)
{
    // Release the old depth stencil view and buffer
    g_pDepthStencilView.Reset();
    g_pDepthStencilBuffer.Reset();

    // Create depth stencil texture
    D3D11_TEXTURE2D_DESC descDepth = {};
    descDepth.Width = width;
    descDepth.Height = height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    descDepth.CPUAccessFlags = 0;
    descDepth.MiscFlags = 0;

    HRESULT hr = g_pd3dDevice->CreateTexture2D(&descDepth, nullptr, &g_pDepthStencilBuffer);
    if (FAILED(hr))
    {
        MessageBox(GetActiveWindow(), L"Failed to create depth stencil texture", L"Error", MB_OK);
        return false;
    }

    // Create the depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
    descDSV.Format = descDepth.Format;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;

    hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencilBuffer.Get(), &descDSV, &g_pDepthStencilView);
    if (FAILED(hr))
    {
        MessageBox(GetActiveWindow(), L"Failed to create depth stencil view", L"Error", MB_OK);
        return false;
    }

    return true;
}

bool RecreateDevice()
{
    // Release all device-dependent resources
    g_pRenderTargetView.Reset();
    g_pDepthStencilView.Reset();
    g_pDepthStencilBuffer.Reset();
    g_pSwapChain.Reset();
    g_pd3dDeviceContext.Reset();
    g_pd3dDevice.Reset();

    // Recreate the device and swap chain
    if (!InitializeDirect3D())
    {
        MessageBox(GetActiveWindow(), L"Failed to reinitialize Direct3D", L"Error", MB_OK);
        return false;
    }

    // Recreate other resources (vertex buffer, shaders, etc.)
    if (!InitializeScene())
    {
        MessageBox(GetActiveWindow(), L"Failed to reinitialize scene", L"Error", MB_OK);
        return false;
    }

    return true;
}

void ResizeDirectXBuffers(HWND hWnd)
{
    if (!g_pSwapChain) return;

    // Release all outstanding references to the swap chain's buffers
    g_pRenderTargetView.Reset();
    g_pDepthStencilView.Reset();
    g_pDepthStencilBuffer.Reset();

    ID3D11RenderTargetView* nullViews[] = { nullptr };
    g_pd3dDeviceContext->OMSetRenderTargets(_countof(nullViews), nullViews, nullptr);
    g_pd3dDeviceContext->Flush();

    RECT rc;
    GetClientRect(hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    HRESULT hr = g_pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr))
    {
        MessageBox(hWnd, L"Failed to resize swap chain buffers", L"Error", MB_OK);
        return;
    }

    // Recreate the render target view
    ComPtr<ID3D11Texture2D> pBackBuffer;
    hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (FAILED(hr))
    {
        MessageBox(hWnd, L"Failed to get back buffer", L"Error", MB_OK);
        return;
    }

    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &g_pRenderTargetView);
    if (FAILED(hr))
    {
        MessageBox(hWnd, L"Failed to create render target view", L"Error", MB_OK);
        return;
    }

    // Recreate the depth stencil view
    if (!CreateDepthStencilView(width, height))
    {
        MessageBox(hWnd, L"Failed to create depth stencil view", L"Error", MB_OK);
        return;
    }

    // Set the new render target and depth stencil view
    g_pd3dDeviceContext->OMSetRenderTargets(1, g_pRenderTargetView.GetAddressOf(), g_pDepthStencilView.Get());

    // Update the viewport
    g_Viewport.Width = static_cast<float>(width);
    g_Viewport.Height = static_cast<float>(height);
    g_Viewport.MinDepth = 0.0f;
    g_Viewport.MaxDepth = 1.0f;
    g_Viewport.TopLeftX = 0;
    g_Viewport.TopLeftY = 0;
    g_pd3dDeviceContext->RSSetViewports(1, &g_Viewport);
}

void ToggleFullscreen(HWND hWnd)
{
    BOOL fullscreen;
    HRESULT hr = g_pSwapChain->GetFullscreenState(&fullscreen, nullptr);
    if (FAILED(hr))
    {
        MessageBox(hWnd, L"Failed to get fullscreen state", L"Error", MB_OK);
        return;
    }

    if (fullscreen)
    {
        // Switch to windowed mode
        hr = g_pSwapChain->SetFullscreenState(FALSE, nullptr);
        if (FAILED(hr))
        {
            MessageBox(hWnd, L"Failed to switch to windowed mode", L"Error", MB_OK);
            return;
        }
        SetWindowLong(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
        SetWindowPos(hWnd, HWND_TOP,
            g_WindowRect.left, g_WindowRect.top,
            g_WindowRect.right - g_WindowRect.left,
            g_WindowRect.bottom - g_WindowRect.top,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
    else
    {
        // Switch to fullscreen mode
        GetWindowRect(hWnd, &g_WindowRect);
        SetWindowLong(hWnd, GWL_STYLE, WS_POPUP);
        hr = g_pSwapChain->SetFullscreenState(TRUE, nullptr);
        if (FAILED(hr))
        {
            MessageBox(hWnd, L"Failed to switch to fullscreen mode", L"Error", MB_OK);
            return;
        }
    }

    g_IsFullscreen = !fullscreen;

    // Recreate render target view and depth stencil view
    ResizeDirectXBuffers(hWnd);
}

void HandleResize()
{
    if (g_ResizePending)
    {
        DWORD currentTime = GetTickCount();
        if (currentTime - g_ResizeLastTime > RESIZE_DELAY_MS)
        {
            ResizeDirectXBuffers(GetActiveWindow());
            g_ResizePending = false;
        }
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            DestroyWindow(hWnd);
        }
        else if (wParam == VK_F11)
        {
            ToggleFullscreen(hWnd);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_ENTERSIZEMOVE:
        g_Resizing = true;
        return 0;
    case WM_EXITSIZEMOVE:
        g_Resizing = false;
        ResizeDirectXBuffers(hWnd);
        return 0;
    case WM_SIZE:
        if (g_pd3dDevice)
        {
            if (wParam == SIZE_MINIMIZED)
            {
                return 0;
            }
            else if (wParam == SIZE_MAXIMIZED || (wParam == SIZE_RESTORED && !g_Resizing))
            {
                ResizeDirectXBuffers(hWnd);
            }
        }
        return 0;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}