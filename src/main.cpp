#include <iostream>
#include <vector>
#include <nvrhi/nvrhi.h>
#include <nvrhi/d3d12.h>
#include <spdlog/fmt/fmt.h>

#include "Core/Window.h"
#include "Core/Buffer.h"
#include "Core/Texture.h"
#include "Core/ComputePass.h"
#include "Utils/Math/Math.h"
#include "Utils/Logger.h"
#include "Utils/GUI.h"
#include "Scene/Camera/Camera.h"

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

using Microsoft::WRL::ComPtr;

// Helper: check HRESULT
#define CHECKHR(x)                                          \
    if (FAILED(x))                                          \
    {                                                       \
        std::cerr << "HRESULT failed: " << #x << std::endl; \
        exit(1);                                            \
    }

class MyMessageCallback : public nvrhi::IMessageCallback
{
public:
    void message(nvrhi::MessageSeverity severity, const char* messageText) override { std::cerr << "[NVRHI] " << messageText << std::endl; }
};

int main()
{
    Logger::init();
    auto logger = Logger::get();

    // -------------------------
    // 1. Init D3D12 Device
    // ------------------------
#ifdef DX12_ENABLE_DEBUG_LAYER
    ID3D12Debug* pdx12Debug = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pdx12Debug))))
        pdx12Debug->EnableDebugLayer();
#endif

    ComPtr<IDXGIFactory4> dxgiFactory;
    CHECKHR(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)));

    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> d3d12Device;

    // Try to find a hardware adapter first
    bool deviceCreated = false;
    for (UINT adapterIndex = 0;; ++adapterIndex)
    {
        if (FAILED(dxgiFactory->EnumAdapters1(adapterIndex, &adapter)))
            break;

        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // Skip software adapters
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        // Try to create device with this adapter
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&d3d12Device))))
        {
            // Convert wide string to regular string for logging
            std::wstring wstr(desc.Description);
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
            std::string str(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, NULL, NULL);
            LOG_INFO("Using hardware adapter: {}", str);
            deviceCreated = true;
            break;
        }

        adapter.Reset();
    }

    // If no hardware adapter worked, try WARP (software renderer)
    if (!deviceCreated)
    {
        LOG_WARN("No hardware adapter found, trying WARP (software renderer)...");
        if (SUCCEEDED(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))))
        {
            CHECKHR(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&d3d12Device)));
            LOG_INFO("Using WARP software adapter");
            deviceCreated = true;
        }
    }

    if (!deviceCreated)
    {
        LOG_ERROR("Failed to create D3D12 device with any adapter!");
        exit(1);
    }

    // -------------------------
    // 2. Create NVRHI Device
    // -------------------------
    nvrhi::d3d12::DeviceDesc deviceDesc;
    deviceDesc.pDevice = d3d12Device.Get();
    MyMessageCallback myCallback;
    deviceDesc.errorCB = &myCallback;

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ComPtr<ID3D12CommandQueue> commandQueue;
    CHECKHR(d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));
    deviceDesc.pGraphicsCommandQueue = commandQueue.Get();
    nvrhi::DeviceHandle nvrhiDevice = nvrhi::d3d12::createDevice(deviceDesc);

    const uint32_t width = 1920;
    const uint32_t height = 1080;
    // Create window with configuration
    Window::desc windowDesc;
    windowDesc.width = width;
    windowDesc.height = height;
    windowDesc.title = "007Renderer";
    windowDesc.enableVSync = false;

    Window window(d3d12Device, commandQueue, windowDesc);
    window.PrepareResources();
    LOG_INFO("Renderer initialized successfully.");

    {
        // -------------------------
        // 3. Prepare shader buffer data
        // -------------------------
        struct PerFrameCB
        {
            uint32_t gWidth;
            uint32_t gHeight;
            float gColor;
            float _padding;
        } perFrameData;

        Camera camera(width, height, float3(0.f, 0.f, 1.f), float3(0.f, 0.f, 0.f), float3(0.f, 1.f, 0.f), glm::radians(45.0f));
        Buffer cbPerFrame, cbCamera;
        Texture textureOut;
        cbPerFrame.initialize(nvrhiDevice, &perFrameData, sizeof(PerFrameCB), nvrhi::ResourceStates::ConstantBuffer, false, true, "PerFrameCB");
        textureOut.initialize(nvrhiDevice, width, height, nvrhi::Format::RGBA32_FLOAT, nvrhi::ResourceStates::UnorderedAccess, true, "TextureOut");
        cbCamera.initialize(nvrhiDevice, &camera.getCameraData(), sizeof(CameraData), nvrhi::ResourceStates::ConstantBuffer, false, true, "Camera");

        // -------------------------
        // 4. Setup shader & dispatch
        // -------------------------
        std::unordered_map<std::string, nvrhi::RefCountPtr<nvrhi::IResource>> resourceMap;
        resourceMap["result"] = nvrhi::ResourceHandle(textureOut.getHandle().operator->());
        resourceMap["PerFrameCB"] = nvrhi::ResourceHandle(cbPerFrame.getHandle().operator->());
        resourceMap["gCamera"] = nvrhi::ResourceHandle(cbCamera.getHandle().operator->());

        ComputePass pass;
        pass.initialize(nvrhiDevice, std::string(PROJECT_SHADER_DIR) + "/hello.slang", "computeMain", resourceMap);

        // -------------------------
        // 5. Setup GUI with original ImGui
        // -------------------------
        bool notDone = true;
        static float gColorSlider = 0.5f; // UI slider value
        static int counter = 0;

        while (notDone)
        {
            HRESULT deviceRemovedReason = d3d12Device->GetDeviceRemovedReason();
            if (FAILED(deviceRemovedReason))
            {
                LOG_ERROR("Device removed: 0x{:08X}", static_cast<uint32_t>(deviceRemovedReason));

                // If in RDP environment, log additional info
                if (deviceRemovedReason == DXGI_ERROR_DEVICE_REMOVED)
                {
                    LOG_ERROR("This error commonly occurs in RDP environments. Consider using software rendering.");
                    LOG_ERROR("The application will now exit. Try running locally or use a different remote desktop solution.");
                }

                notDone = false;
                break;
            }
            nvrhiDevice->runGarbageCollection();

            perFrameData.gWidth = width;
            perFrameData.gHeight = height;
            perFrameData.gColor = gColorSlider;
            cbPerFrame.updateData(nvrhiDevice, &perFrameData, sizeof(PerFrameCB));
            cbCamera.updateData(nvrhiDevice, &camera.getCameraData(), sizeof(CameraData));

            // Dispatch compute shader
            pass.dispatchThreads(nvrhiDevice, width, height, 1);

            // Set texture for display
            ID3D12Resource* d3d12Texture = static_cast<ID3D12Resource*>(textureOut.getHandle()->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource));
            window.SetDisplayTexture(d3d12Texture);

            // Custom ImGui content before window render
            bool renderResult = window.RenderBegin();
            if (!renderResult)
            {
                notDone = false;
                break;
            }

            ImGuiIO& io = GUI::GetIO();
            GUI::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
            GUI::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.3f, io.DisplaySize.y * 0.5f), ImGuiCond_Once);

            GUI::Begin("Settings");
            GUI::Text("This is some useful text.");
            GUI::SliderFloat("gColor", &gColorSlider, 0.0f, 1.0f);
            if (GUI::Button("Button"))
                counter++;
            GUI::SameLine();
            GUI::Text("counter = %d", counter);
            GUI::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / GUI::GetIO().Framerate, GUI::GetIO().Framerate);
            camera.renderUI();
            camera.handleInput();
            GUI::End();

            if (GUI::IsKeyPressed(ImGuiKey_Escape))
                notDone = false; // Exit on Escape key

            // Finish rendering
            window.RenderEnd();
        }
    }

    window.CleanupResources();
    nvrhiDevice->waitForIdle();
    nvrhiDevice = nullptr;
    LOG_INFO("Renderer shutdown successfully.");
    spdlog::shutdown();
    return 0;
}