#include <windows.h>
#include <d3d11_4.h>
#include <d3dcompiler.h>
#include <directxmath.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

// Global Declarations
IDXGISwapChain* SwapChain;
ID3D11Device* d3d11Device;
ID3D11DeviceContext* d3d11DevCon;
ID3D11RenderTargetView* renderTargetView;

float red = 0.0f;
float green = 0.0f;
float blue = 0.0f;
int colormodr = 1;
int colormodg = 1;
int colormodb = 1;

constexpr LPCWSTR WndClassName = L"DirectXWindow";
HWND hwnd = nullptr;

constexpr int Width = 300;
constexpr int Height = 300;

// Function Prototypes
bool InitializeDirect3d11App(HINSTANCE hInstance);
void ReleaseObjects();
bool InitScene();
void UpdateScene();
void DrawScene();

bool InitializeWindow(HINSTANCE hInstance, int ShowWnd, int width, int height, bool windowed);
int messageloop();

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
    if (!InitializeWindow(hInstance, nShowCmd, Width, Height, true))
    {
        MessageBoxW(nullptr, L"Window Initialization - Failed", L"Error", MB_OK);
        return 0;
    }

    if (!InitializeDirect3d11App(hInstance))
    {
        MessageBoxW(nullptr, L"Direct3D Initialization - Failed", L"Error", MB_OK);
        return 0;
    }

    if (!InitScene())
    {
        MessageBoxW(nullptr, L"Scene Initialization - Failed", L"Error", MB_OK);
        return 0;
    }

    messageloop();

    ReleaseObjects();

    return 0;
}

bool InitializeWindow(HINSTANCE hInstance, int ShowWnd, int width, int height, bool windowed)
{
    WNDCLASSEXW wc;

    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = WndClassName;
    wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);

    if (!RegisterClassExW(&wc))
    {
        MessageBoxW(nullptr, L"Error registering class", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    hwnd = CreateWindowExW(
        0,
        WndClassName,
        L"DirectX Window",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd)
    {
        MessageBoxW(nullptr, L"Error creating window", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    ShowWindow(hwnd, ShowWnd);
    UpdateWindow(hwnd);

    return true;
}

bool InitializeDirect3d11App(HINSTANCE hInstance)
{
    //DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    //swapChainDesc.BufferCount = 1;
    //swapChainDesc.BufferDesc.Width = Width;
    //swapChainDesc.BufferDesc.Height = Height;
    //swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    //swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    //swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    //swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    //swapChainDesc.OutputWindow = hwnd;
    //swapChainDesc.SampleDesc.Count = 1;
    //swapChainDesc.SampleDesc.Quality = 0;
    //swapChainDesc.Windowed = TRUE;

    // For Triple Buffer
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Width = Width;
    swapChainDesc.BufferDesc.Height = Height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;
    // Latest SwapEffect Model.
    // Flip model swapchains have a few additional requirements on top of blt swapchains :
    //    1. The buffer count must be at least 2.
    //    2. After Present calls, the back buffer needs to explicitly be re - bound to the D3D11 immediate context before it can be used again.
    //    3. After calling SetFullscreenState, the app must call ResizeBuffers before Present.
    //    4. MSAA swapchains are not directly supported in flip model, so the app will need to do an MSAA resolve before issuing the Present.
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);

    D3D_FEATURE_LEVEL featureLevel;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevels, numFeatureLevels,
        D3D11_SDK_VERSION, &swapChainDesc, &SwapChain, &d3d11Device, &featureLevel, &d3d11DevCon);

    if (FAILED(hr))
    {
        return false;
    }

    ID3D11Texture2D* BackBuffer;
    SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&BackBuffer);
    d3d11Device->CreateRenderTargetView(BackBuffer, nullptr, &renderTargetView);
    BackBuffer->Release();

    d3d11DevCon->OMSetRenderTargets(1, &renderTargetView, nullptr);

    return true;
}

void ReleaseObjects()
{
    if (SwapChain)
        SwapChain->Release();
    if (d3d11Device)
        d3d11Device->Release();
    if (d3d11DevCon)
        d3d11DevCon->Release();
    if (renderTargetView)
        renderTargetView->Release();
}

bool InitScene()
{
    return true;
}

void UpdateScene()
{
    red += colormodr * 0.00005f;
    green += colormodg * 0.00002f;
    blue += colormodb * 0.00001f;

    if (red >= 1.0f || red <= 0.0f)
        colormodr *= -1;
    if (green >= 1.0f || green <= 0.0f)
        colormodg *= -1;
    if (blue >= 1.0f || blue <= 0.0f)
        colormodb *= -1;
}

void DrawScene()
{
    const float bgColor[4] = { red, green, blue, 1.0f };
    d3d11DevCon->ClearRenderTargetView(renderTargetView, bgColor);

    SwapChain->Present(0, 0);

    // Rebinding the back buffer
    ID3D11Texture2D* backBuffer = nullptr;
    SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
    d3d11Device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
    backBuffer->Release();
    d3d11DevCon->OMSetRenderTargets(1, &renderTargetView, nullptr);
}

int messageloop()
{
    MSG msg = {};
    while (true)
    {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        else
        {
            UpdateScene();
            DrawScene();
        }
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    // Full-screen transitions and buffer resizing
    case WM_SIZE:
        if (SwapChain)
        {
            d3d11DevCon->OMSetRenderTargets(0, 0, 0);
            renderTargetView->Release();

            HRESULT hr = SwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
            if (SUCCEEDED(hr))
            {
                ID3D11Texture2D* backBuffer = nullptr;
                SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
                d3d11Device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
                backBuffer->Release();
                d3d11DevCon->OMSetRenderTargets(1, &renderTargetView, nullptr);
            }
        }
        return 0;
}
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}