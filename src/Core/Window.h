#pragma once
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <tchar.h>
#include <cstdint>
#include <string>

#include "DescriptorHeapAllocator.h"
#include "Utils/Math/Math.h"

using Microsoft::WRL::ComPtr;

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

struct FrameContext
{
    ID3D12CommandAllocator* CommandAllocator;
    UINT64 FenceValue;
};

class Window
{
public:
    // Window configuration configuration
    struct desc
    {
        uint32_t width = 1920;               ///< The width of the client area size.
        uint32_t height = 1080;              ///< The height of the client area size.
        std::string title = "Falcor Sample"; ///< Window title.
        bool enableVSync = false;            ///< Controls vertical-sync.
    };

    Window(ComPtr<ID3D12Device> device, ComPtr<ID3D12CommandQueue> commandQueue, const desc& windowDesc);
    ~Window() {};
    void PrepareResources();
    void CleanupResources();
    bool RenderBegin();
    void RenderEnd();
    void SetDisplayTexture(ID3D12Resource* texture);
    ImTextureID GetDisplayTextureImGuiHandle() const;
    ID3D12Resource* GetCurrentDisplayTexture() const;
    uint2 GetWindowSize() const;

    // Forward declarations of helper functions
    bool CreateDeviceD3D(HWND hWnd);
    void CleanupDeviceD3D();
    FrameContext* WaitForNextFrameResources();

private:
    // Internal state
    float mMainScale;
    HWND mHwnd;
    WNDCLASSEXW mWc;
    ImGuiIO* mpIo;
    bool mEnableVSync;

    // Display texture and descriptor handles
    ID3D12Resource* mpCurrentDisplayTexture = nullptr;
    ImTextureID mDisplayImGuiHandle = (ImTextureID)0;
    D3D12_CPU_DESCRIPTOR_HANDLE mDisplaySrvCpuHandle = {};
    D3D12_GPU_DESCRIPTOR_HANDLE mDisplaySrvGpuHandle = {};
};