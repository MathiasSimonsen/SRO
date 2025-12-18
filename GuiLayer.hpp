#pragma once

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <string>

// guilayer makes window and directx init idk
class GuiLayer {
public:
    GuiLayer();
    ~GuiLayer();

    // init window and directx
    bool Init(const std::string& title, int width, int height);
    
    // starts new frame call before render
    void BeginFrame();
    
    // ends frame and renders
    void EndFrame();
    
    // check if window should close
    bool ShouldClose();

    // toggle click through transparent
    void SetClickThrough(bool enable);
    
    void ToggleMenu();
    bool requestMenuToggle = false;

    // get HWND
    HWND GetHwnd() { return hwnd; }
    ID3D11Device* GetDevice() { return g_pd3dDevice; }
    ID3D11DeviceContext* GetDeviceContext() { return g_pd3dDeviceContext; }

    // help handle win32 msgs
    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    // directx vars
    HWND hwnd;
    WNDCLASSEX wc;
    ID3D11Device* g_pd3dDevice = nullptr;
    ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
    IDXGISwapChain* g_pSwapChain = nullptr;
    ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
    
    bool CreateDeviceD3D(HWND hWnd);
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();
    
    // close flag
    bool shouldClose = false;
};
