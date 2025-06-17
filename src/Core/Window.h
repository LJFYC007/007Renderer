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

using namespace Microsoft::WRL;
using Microsoft::WRL::ComPtr;

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

// Config for example app
static const int APP_NUM_FRAMES_IN_FLIGHT = 2;
static const int APP_NUM_BACK_BUFFERS = 2;
static const int APP_SRV_HEAP_SIZE = 64;

struct FrameContext
{
    ID3D12CommandAllocator* CommandAllocator;
    UINT64 FenceValue;
};

// Data
static FrameContext g_frameContext[APP_NUM_FRAMES_IN_FLIGHT] = {};
static UINT g_frameIndex = 0;

static ComPtr<ID3D12Device> g_pd3dDevice = nullptr;
static ID3D12DescriptorHeap* g_pd3dRtvDescHeap = nullptr;
static ID3D12DescriptorHeap* g_pd3dSrvDescHeap = nullptr;
static ExampleDescriptorHeapAllocator g_pd3dSrvDescHeapAlloc;
static ComPtr<ID3D12CommandQueue> g_pd3dCommandQueue = nullptr;
static ID3D12GraphicsCommandList* g_pd3dCommandList = nullptr;
static ID3D12Fence* g_fence = nullptr;
static HANDLE g_fenceEvent = nullptr;
static UINT64 g_fenceLastSignaledValue = 0;
static IDXGISwapChain3* g_pSwapChain = nullptr;
static bool g_SwapChainOccluded = false;
static HANDLE g_hSwapChainWaitableObject = nullptr;
static ID3D12Resource* g_mainRenderTargetResource[APP_NUM_BACK_BUFFERS] = {};
static D3D12_CPU_DESCRIPTOR_HANDLE g_mainRenderTargetDescriptor[APP_NUM_BACK_BUFFERS] = {};

void CreateRenderTarget();
void CleanupRenderTarget();
void WaitForLastSubmittedFrame();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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

    // Forward declarations of helper functions
    bool CreateDeviceD3D(HWND hWnd);
    void CleanupDeviceD3D();
    FrameContext* WaitForNextFrameResources();

private:
    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Internal state
    float main_scale;
    HWND hwnd;
    WNDCLASSEXW wc;
    ImGuiIO* io;
};