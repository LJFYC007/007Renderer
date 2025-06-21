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
    /**
     * Window mode
     */
    enum class WindowMode
    {
        Normal,     ///< Normal window.
        Minimized,  ///< Minimized window.
        Fullscreen, ///< Fullscreen window.
    };

    /**
     * Window configuration configuration
     */
    struct desc
    {
        uint32_t width = 1920;                ///< The width of the client area size.
        uint32_t height = 1080;               ///< The height of the client area size.
        std::string title = "Falcor Sample";  ///< Window title.
        WindowMode mode = WindowMode::Normal; ///< Window mode. In full screen mode, width and height will be ignored.
        bool resizableWindow = true;          ///< Allow the user to resize the window.
        bool enableVSync = false;             ///< Controls vertical-sync.
    };

    Window(ComPtr<ID3D12Device> device, ComPtr<ID3D12CommandQueue> commandQueue);
    ~Window() {};
    void PrepareResources();
    void CleanupResources();
    bool Render();
    void SetDisplayTexture(ID3D12Resource* texture);

    // Forward declarations of helper functions
    bool CreateDeviceD3D(HWND hWnd);
    void CleanupDeviceD3D();
    FrameContext* WaitForNextFrameResources();

private:
    // Internal state
    float main_scale;
    HWND hwnd;
    WNDCLASSEXW wc;
    ImGuiIO* io;

    // Display texture and descriptor handles
    ID3D12Resource* m_CurrentDisplayTexture = nullptr;
    ImTextureID m_DisplayImGuiHandle = (ImTextureID)0;
    D3D12_CPU_DESCRIPTOR_HANDLE m_DisplaySrvCpuHandle = {};
    D3D12_GPU_DESCRIPTOR_HANDLE m_DisplaySrvGpuHandle = {};
};