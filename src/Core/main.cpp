#pragma once
#include <iostream>
#include <vector>
#include <nvrhi/nvrhi.h>
#include <nvrhi/d3d12.h>
#include <spdlog/fmt/fmt.h>

#include "Window.h"
#include "Buffer.h"
#include "Texture.h"
#include "ComputePass.h"
#include "Utils/Logger.h"
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
    // -------------------------
    // 1. Init D3D12 Device
    // ------------------------
#ifdef DX12_ENABLE_DEBUG_LAYER
    ID3D12Debug* pdx12Debug = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pdx12Debug))))
        pdx12Debug->EnableDebugLayer();
#endif -

    ComPtr<IDXGIFactory4> dxgiFactory;
    CHECKHR(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)));

    ComPtr<IDXGIAdapter1> adapter;
    CHECKHR(dxgiFactory->EnumAdapters1(0, &adapter));

    ComPtr<ID3D12Device> d3d12Device;
    CHECKHR(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&d3d12Device)));

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

    Logger::init();
    auto logger = Logger::get();
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

        Camera camera;
        Buffer cbPerFrame, cbCamera;
        Texture textureOut;
        cbPerFrame.initialize(nvrhiDevice, &perFrameData, sizeof(PerFrameCB), nvrhi::ResourceStates::ConstantBuffer, false, true, "PerFrameCB");
        textureOut.initialize(nvrhiDevice, width, height, nvrhi::Format::RGBA32_FLOAT, nvrhi::ResourceStates::UnorderedAccess, true, "TextureOut");
        cbCamera.initialize(nvrhiDevice, &camera, sizeof(Camera), nvrhi::ResourceStates::ConstantBuffer, false, true, "Camera");

        // -------------------------
        // 4. Setup shader & dispatch
        // -------------------------
        std::unordered_map<std::string, nvrhi::RefCountPtr<nvrhi::IResource>> resourceMap;
        resourceMap["result"] = nvrhi::RefCountPtr<nvrhi::IResource>(textureOut.getHandle().operator->());
        resourceMap["PerFrameCB"] = nvrhi::RefCountPtr<nvrhi::IResource>(cbPerFrame.getHandle().operator->());
        resourceMap["gCamera"] = nvrhi::RefCountPtr<nvrhi::IResource>(cbCamera.getHandle().operator->());
        ComputePass pass;
        pass.initialize(nvrhiDevice, std::string(PROJECT_SHADER_DIR) + "/hello.slang", "computeMain", resourceMap);

        // -------------------------
        // 5. Imgui with real-time compute
        // -------------------------
        bool notDone = true;
        static float gColorSlider = 0.0f; // UI slider value
        static int counter = 0;

        while (notDone)
        {
            nvrhiDevice->runGarbageCollection();

            perFrameData.gWidth = width;
            perFrameData.gHeight = height;
            perFrameData.gColor = gColorSlider;
            cbPerFrame.updateData(nvrhiDevice, &perFrameData, sizeof(PerFrameCB));

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

            ImGuiIO& io = ImGui::GetIO();
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.3f, io.DisplaySize.y * 0.5f), ImGuiCond_Once);
            ImGui::Begin("Settings");
            ImGui::Text("This is some useful text.");
            ImGui::SliderFloat("gColor", &gColorSlider, 0.0f, 1.0f);
            if (ImGui::Button("Button"))
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();

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