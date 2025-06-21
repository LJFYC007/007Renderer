#include "Window.h"
#include "Buffer.h"
#include "ComputePass.h"

#include <iostream>
#include <vector>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <slang.h>
#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <nvrhi/nvrhi.h>
#include <nvrhi/d3d12.h>

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

using namespace Microsoft::WRL;
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
    {
        // -------------------------
        // 3. Prepare shader buffer data
        // -------------------------
        const uint32_t elementCount = 8;
        std::vector<float> inputA(elementCount, 1.0f);
        std::vector<float> inputB(elementCount, 5.0f);

        Buffer bufA, bufB, bufOut;
        bufA.initialize(nvrhiDevice, inputA.data(), inputA.size() * sizeof(float), nvrhi::ResourceStates::ShaderResource, false, "BufferA");
        bufB.initialize(nvrhiDevice, inputB.data(), inputB.size() * sizeof(float), nvrhi::ResourceStates::ShaderResource, false, "BufferB");
        bufOut.initialize(nvrhiDevice, nullptr, inputA.size() * sizeof(float), nvrhi::ResourceStates::UnorderedAccess, true, "BufferOut");

        // -------------------------
        // 4. Setup shader & dispatch
        // -------------------------
        std::unordered_map<std::string, nvrhi::BufferHandle> bufferMap;
        bufferMap["buffer0"] = bufA.getHandle();
        bufferMap["buffer1"] = bufB.getHandle();
        bufferMap["result"] = bufOut.getHandle();

        ComputePass pass;
        pass.initialize(nvrhiDevice, "shaders/hello.slang", "computeMain", bufferMap);

        pass.dispatch(nvrhiDevice, elementCount);
        nvrhiDevice->waitForIdle();

        // -------------------------
        // 5. Read back and print result
        // -------------------------
        auto readResult = bufOut.readback(nvrhiDevice, nvrhiDevice->createCommandList());
        const float* resultData = reinterpret_cast<const float*>(readResult.data());
        for (int i = 0; i < elementCount; ++i)
            std::cout << resultData[i] << " ";

        // Generate texture
        nvrhi::TextureDesc textureDesc;
        textureDesc.width = 1920;
        textureDesc.height = 1080;
        textureDesc.format = nvrhi::Format::RGBA8_UNORM;
        textureDesc.isRenderTarget = false;
        textureDesc.isUAV = true;
        textureDesc.debugName = "TestDisplayTexture";
        textureDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        textureDesc.keepInitialState = true;

        nvrhi::TextureHandle nvrhiTexture = nvrhiDevice->createTexture(textureDesc);

        nvrhi::Color color(0.0f, 0.5f, 1.0f, 1.0f); // Clear color for the texture
        nvrhi::TextureSubresourceSet allSubresources;

        auto clearCmd = nvrhiDevice->createCommandList();
        clearCmd->open();
        clearCmd->beginTrackingTextureState(nvrhiTexture, allSubresources, nvrhi::ResourceStates::ShaderResource);
        clearCmd->setTextureState(nvrhiTexture, allSubresources, nvrhi::ResourceStates::UnorderedAccess);
        clearCmd->clearTextureFloat(nvrhiTexture, allSubresources, color);
        clearCmd->setPermanentTextureState(nvrhiTexture, nvrhi::ResourceStates::ShaderResource);
        clearCmd->close();
        nvrhiDevice->executeCommandList(clearCmd);
        nvrhiDevice->waitForIdle();

        // -------------------------
        // 6. Imgui
        // -------------------------

        bool notDone = true;
        while (notDone)
        {
            // Main loop
            ID3D12Resource* d3d12Texture = static_cast<ID3D12Resource*>(nvrhiTexture->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource));
            window.SetDisplayTexture(d3d12Texture);
            notDone = window.Render();
        }
    }

    window.CleanupResources();
    nvrhiDevice->waitForIdle();
    nvrhiDevice = nullptr;
    return 0;
}