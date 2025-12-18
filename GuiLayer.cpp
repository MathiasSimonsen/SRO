#include "GuiLayer.hpp"
#include <iostream>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

// need this for imgui idk
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// pointer thingy
static GuiLayer* g_GuiLayer = nullptr;

LRESULT WINAPI GuiLayer::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_HOTKEY:
        if (wParam == 1) {
            // global hotkey triggered
            if (g_GuiLayer) g_GuiLayer->ToggleMenu();
        }
        return 0;
    case WM_SIZE:
        if (g_GuiLayer && g_GuiLayer->g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            g_GuiLayer->CleanupRenderTarget();
            g_GuiLayer->g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            g_GuiLayer->CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // disable alt application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

GuiLayer::GuiLayer() {
    g_GuiLayer = this;
}

GuiLayer::~GuiLayer() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);
}

bool GuiLayer::Init(const std::string& title, int width, int height) {
    // make app window win32
    wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("TrashDetectionApp"), NULL };
    ::RegisterClassEx(&wc);
    
    // ws_popup for borderless topmost for overlay
    // ws_ex_toolwindow removed alt tab visible
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    hwnd = ::CreateWindowEx(WS_EX_TOPMOST, wc.lpszClassName, _T(title.c_str()), 
                            WS_POPUP, 0, 0, screenWidth, screenHeight, NULL, NULL, wc.hInstance, NULL);

    // DWM Setup
    MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    
    // init direct3d
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return false;
    }

    // show window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // setup imgui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    // setup platform renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    
    // register global hotkey insert
    if (!RegisterHotKey(hwnd, 1, 0, VK_INSERT)) {
        std::cerr << "Global Hotkey Failed" << std::endl;
    }

    return true;
}

void GuiLayer::BeginFrame() {
    // poll messages
    MSG msg;
    while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
        if (msg.message == WM_QUIT)
            shouldClose = true;
    }
    
    if (shouldClose) return;

    // start imgui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void GuiLayer::EndFrame() {
    // rendering
    ImGui::Render();
    
    // transparent clear color alpha 0
    const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; 
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
    
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // present vsync on 1
    g_pSwapChain->Present(1, 0); 
}

bool GuiLayer::ShouldClose() {
    return shouldClose;
}

void GuiLayer::SetClickThrough(bool enable) {
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (enable) {
        // overlay mode add layered and transparent click through
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST);
    } else {
        // menu mode remove transparent
        SetWindowLong(hwnd, GWL_EXSTYLE, (exStyle & ~WS_EX_TRANSPARENT) | WS_EX_LAYERED | WS_EX_TOPMOST);
        
        // force focus
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
    }
}

void GuiLayer::ToggleMenu() {
    requestMenuToggle = true; 
}

// helpers
bool GuiLayer::CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void GuiLayer::CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void GuiLayer::CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void GuiLayer::CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}
