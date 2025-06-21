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

    Window window(d3d12Device, commandQueue);
    window.PrepareResources();

    Logger::init();
    auto logger = Logger::get();
    LOG_INFO("Renderer initialized successfully.");

    {
        // -------------------------
        // 3. Prepare shader buffer data
        // -------------------------
        const uint32_t width = 1920;
        const uint32_t height = 1080;
        const uint32_t elementCount = width * height;
        std::vector<float> inputA(elementCount * 3, 0.1f);
        std::vector<float> inputB(elementCount * 3, 0.5f);

        Buffer bufA, bufB;
        Texture textureOut;
        bufA.initialize(nvrhiDevice, inputA.data(), inputA.size() * sizeof(float), nvrhi::ResourceStates::ShaderResource, false, "BufferA");
        bufB.initialize(nvrhiDevice, inputB.data(), inputB.size() * sizeof(float), nvrhi::ResourceStates::ShaderResource, false, "BufferB");
        textureOut.initialize(
            nvrhiDevice, width, height, nvrhi::Format::RGBA32_FLOAT, nvrhi::ResourceStates::UnorderedAccess, true, "TextureOut"
        );

        // -------------------------
        // 4. Setup shader & dispatch
        // -------------------------
        std::unordered_map<std::string, nvrhi::RefCountPtr<nvrhi::IResource>> resourceMap;
        resourceMap["buffer0"] = nvrhi::RefCountPtr<nvrhi::IResource>(bufA.getHandle().operator->());
        resourceMap["buffer1"] = nvrhi::RefCountPtr<nvrhi::IResource>(bufB.getHandle().operator->());
        resourceMap["result"] = nvrhi::RefCountPtr<nvrhi::IResource>(textureOut.getHandle().operator->());

        ComputePass pass;
        pass.initialize(nvrhiDevice, std::string(PROJECT_SHADER_DIR) + "/hello.slang", "computeMain", resourceMap);
        pass.dispatchThreads(nvrhiDevice, width, height, 1);
        nvrhiDevice->waitForIdle();

        // -------------------------
        // 5. Imgui
        // -------------------------

        bool notDone = true;
        while (notDone)
        {
            // Main loop
            ID3D12Resource* d3d12Texture =
                static_cast<ID3D12Resource*>(textureOut.getHandle()->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource));
            window.SetDisplayTexture(d3d12Texture);
            notDone = window.Render();
        }
    }

    window.CleanupResources();
    nvrhiDevice->waitForIdle();
    nvrhiDevice = nullptr;
    LOG_INFO("Renderer shutdown successfully.");
    spdlog::shutdown();
    return 0;
}