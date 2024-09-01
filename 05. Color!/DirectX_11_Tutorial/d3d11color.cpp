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
ComPtr<ID3D11VertexShader> g_pVertexShader;
ComPtr<ID3D11PixelShader> g_pPixelShader;
ComPtr<ID3D11InputLayout> g_pVertexLayout;

// Window constants
constexpr LPCTSTR WndClassName = L"DirectXWindow";
constexpr int Width = 800;
constexpr int Height = 600;

// Function Prototypes
bool InitializeWindow(HINSTANCE hInstance, int nCmdShow);
bool InitializeDirect3D();
void CleanupDirect3D();
bool InitializeScene();
void UpdateScene();
void DrawScene();
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

// Vertex Structure
struct Vertex
{
    XMFLOAT3 Position;
};

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

    HWND hWnd = CreateWindow(WndClassName, L"DirectX 11 Application", WS_OVERLAPPEDWINDOW,
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

    // Create Direct3D 11.1 device and context
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevels, numFeatureLevels, D3D11_SDK_VERSION,
        reinterpret_cast<ID3D11Device**>(g_pd3dDevice.GetAddressOf()),
        &featureLevel,
        reinterpret_cast<ID3D11DeviceContext**>(g_pd3dDeviceContext.GetAddressOf()));

    if (FAILED(hr))
    {
        MessageBox(nullptr, L"D3D11CreateDevice Failed", L"Error", MB_OK);
        return false;
    }

    if (featureLevel != D3D_FEATURE_LEVEL_11_0 && featureLevel != D3D_FEATURE_LEVEL_11_1)
    {
        MessageBox(nullptr, L"Direct3D Feature Level 11 unsupported", L"Error", MB_OK);
        return false;
    }

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
    sd.BufferCount = 2;  // Double buffering for flip model
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    hr = dxgiFactory->CreateSwapChainForHwnd(g_pd3dDevice.Get(), GetActiveWindow(), &sd, nullptr, nullptr, &g_pSwapChain);
    if (FAILED(hr))
        return false;

    // Disable Alt+Enter fullscreen toggle
    dxgiFactory->MakeWindowAssociation(GetActiveWindow(), DXGI_MWA_NO_ALT_ENTER);

    // Create a render target view
    ComPtr<ID3D11Texture2D> pBackBuffer;
    hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (FAILED(hr))
        return false;

    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &g_pRenderTargetView);
    if (FAILED(hr))
        return false;

    g_pd3dDeviceContext->OMSetRenderTargets(1, g_pRenderTargetView.GetAddressOf(), nullptr);

    D3D11_VIEWPORT vp;
    vp.Width = static_cast<float>(Width);
    vp.Height = static_cast<float>(Height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pd3dDeviceContext->RSSetViewports(1, &vp);

    return true;
}

void CleanupDirect3D()
{
    if (g_pd3dDeviceContext) g_pd3dDeviceContext->ClearState();
}

bool InitializeScene()
{
    HRESULT hr = S_OK;

    // Compile the vertex shader
    ComPtr<ID3DBlob> pVSBlob;
    hr = D3DCompileFromFile(L"VertexShader.hlsl", nullptr, nullptr, "main", "vs_5_0", 0, 0, &pVSBlob, nullptr);
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
    hr = D3DCompileFromFile(L"PixelShader.hlsl", nullptr, nullptr, "main", "ps_5_0", 0, 0, &pPSBlob, nullptr);
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
        { XMFLOAT3(0.0f,  0.5f, 0.5f) },
        { XMFLOAT3(0.5f, -0.5f, 0.5f) },
        { XMFLOAT3(-0.5f, -0.5f, 0.5f) },
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(Vertex) * 3;
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

    // Set primitive topology
    g_pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    return true;
}

void UpdateScene()
{
    // Update logic here
}

void DrawScene()
{
    // Clear the back buffer
    float ClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f }; // RGBA
    g_pd3dDeviceContext->ClearRenderTargetView(g_pRenderTargetView.Get(), ClearColor);

    // Render a triangle
    g_pd3dDeviceContext->VSSetShader(g_pVertexShader.Get(), nullptr, 0);
    g_pd3dDeviceContext->PSSetShader(g_pPixelShader.Get(), nullptr, 0);
    g_pd3dDeviceContext->Draw(3, 0);

    // Present the information rendered to the back buffer to the front buffer (the screen)
    g_pSwapChain->Present(1, 0);  // Use vsync

    // Rebind the back buffer
    ComPtr<ID3D11Texture2D> pBackBuffer;
    HRESULT hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (SUCCEEDED(hr))
    {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &g_pRenderTargetView);
        g_pd3dDeviceContext->OMSetRenderTargets(1, g_pRenderTargetView.GetAddressOf(), nullptr);
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
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        if (g_pSwapChain)
        {
            RECT rc;
            GetClientRect(hWnd, &rc);
            UINT width = rc.right - rc.left;
            UINT height = rc.bottom - rc.top;

            g_pd3dDeviceContext->OMSetRenderTargets(0, 0, 0);
            g_pRenderTargetView.Reset();

            HRESULT hr = g_pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
            if (SUCCEEDED(hr))
            {
                ComPtr<ID3D11Texture2D> pBuffer;
                hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBuffer));
                if (SUCCEEDED(hr))
                {
                    hr = g_pd3dDevice->CreateRenderTargetView(pBuffer.Get(), nullptr, &g_pRenderTargetView);
                    if (SUCCEEDED(hr))
                    {
                        g_pd3dDeviceContext->OMSetRenderTargets(1, g_pRenderTargetView.GetAddressOf(), nullptr);

                        // Update the viewport
                        D3D11_VIEWPORT vp;
                        vp.Width = static_cast<float>(width);
                        vp.Height = static_cast<float>(height);
                        vp.MinDepth = 0.0f;
                        vp.MaxDepth = 1.0f;
                        vp.TopLeftX = 0;
                        vp.TopLeftY = 0;
                        g_pd3dDeviceContext->RSSetViewports(1, &vp);
                    }
                }
            }
        }
        return 0;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}