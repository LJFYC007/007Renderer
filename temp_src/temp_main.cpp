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
    // -------------------------
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

    // -------------------------
    // 3. Use Slang to compile DXIL
    // -------------------------
    Slang::ComPtr<slang::IGlobalSession> slangSession;
    slang::createGlobalSession(slangSession.writeRef());

    slang::SessionDesc sessionDesc = {};
    sessionDesc.searchPaths = nullptr;
    sessionDesc.searchPathCount = 0;

    Slang::ComPtr<slang::ISession> slangCompileSession;
    slangSession->createSession(sessionDesc, slangCompileSession.writeRef());

    Slang::ComPtr<slang::ICompileRequest> request;
    slangCompileSession->createCompileRequest(request.writeRef());

    int targetIndex = request->addCodeGenTarget(SLANG_DXIL);
    SlangProfileID profile = slangSession->findProfile("cs_6_2");
    request->setTargetProfile(targetIndex, profile);
    request->setTargetFlags(targetIndex, 0); // optional: no extra flags
    request->setMatrixLayoutMode(SLANG_MATRIX_LAYOUT_ROW_MAJOR);

    int tuIndex = request->addTranslationUnit(SLANG_SOURCE_LANGUAGE_SLANG, "unit");
    request->addTranslationUnitSourceFile(tuIndex, "shaders/hello.slang");
    int entryPointIndex = request->addEntryPoint(tuIndex, "computeMain", SLANG_STAGE_COMPUTE);

    if (request->compile() != SLANG_OK)
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        request->getDiagnosticOutputBlob(diagnostics.writeRef());
        std::cerr << "[Slang] Compile error:\n" << (const char*)diagnostics->getBufferPointer();
        return 1;
    }

    Slang::ComPtr<slang::IBlob> shaderBlob;
    SlangResult blobResult = request->getEntryPointCodeBlob(entryPointIndex, targetIndex, shaderBlob.writeRef());
    if (SLANG_FAILED(blobResult) || shaderBlob == nullptr)
    {
        std::cerr << "[Slang] Failed to get target code blob." << std::endl;
        return 1;
    }

    // -------------------------
    // 4. Prepare shader buffer data
    // -------------------------
    const uint32_t elementCount = 8;
    std::vector<float> inputA(elementCount, 1.0f);
    std::vector<float> inputB(elementCount, 5.0f);

    auto createBuffer = [&](const void* data, uint64_t size, nvrhi::ResourceStates initState, bool isOutput) -> nvrhi::BufferHandle
    {
        nvrhi::BufferDesc desc;
        desc.byteSize = size;
        desc.debugName = "Buffer";
        desc.cpuAccess = nvrhi::CpuAccessMode::None;
        desc.structStride = sizeof(float);

        if (isOutput)
        {
            desc.canHaveUAVs = true;
            desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
        }
        else
        {
            desc.initialState = nvrhi::ResourceStates::ShaderResource;
        }

        auto buffer = nvrhiDevice->createBuffer(desc);
        if (data)
        {
            auto uploadCmd = nvrhiDevice->createCommandList();
            uploadCmd->open();
            uploadCmd->beginTrackingBufferState(buffer, initState);
            uploadCmd->writeBuffer(buffer, data, size);
            uploadCmd->close();
            nvrhiDevice->executeCommandList(uploadCmd);
        }
        return buffer;
    };

    auto bufA = createBuffer(inputA.data(), inputA.size() * sizeof(float), nvrhi::ResourceStates::ShaderResource, false);
    auto bufB = createBuffer(inputB.data(), inputB.size() * sizeof(float), nvrhi::ResourceStates::ShaderResource, false);
    auto bufOut = createBuffer(nullptr, inputA.size() * sizeof(float), nvrhi::ResourceStates::UnorderedAccess, true);

    // -------------------------
    // 5. Setup shader & dispatch
    // -------------------------
    nvrhi::ShaderDesc shaderDesc;
    shaderDesc.entryName = "computeMain";
    shaderDesc.shaderType = nvrhi::ShaderType::Compute;
    nvrhi::ShaderHandle computeShader = nvrhiDevice->createShader(shaderDesc, shaderBlob->getBufferPointer(), shaderBlob->getBufferSize());

    nvrhi::BindingLayoutDesc layoutDesc;
    layoutDesc.visibility = nvrhi::ShaderType::Compute;
    layoutDesc.addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(0)); // buffer0
    layoutDesc.addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(1)); // buffer1
    layoutDesc.addItem(nvrhi::BindingLayoutItem::StructuredBuffer_UAV(2)); // result

    auto bindingLayout = nvrhiDevice->createBindingLayout(layoutDesc);

    nvrhi::BindingSetDesc bindingSetDesc;
    bindingSetDesc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(0, bufA));
    bindingSetDesc.addItem(nvrhi::BindingSetItem::StructuredBuffer_SRV(1, bufB));
    bindingSetDesc.addItem(nvrhi::BindingSetItem::StructuredBuffer_UAV(2, bufOut));

    nvrhi::BindingSetHandle bindingSet = nvrhiDevice->createBindingSet(bindingSetDesc, bindingLayout);

    nvrhi::ComputePipelineDesc pipelineDesc;
    pipelineDesc.bindingLayouts = {bindingLayout};
    pipelineDesc.setComputeShader(computeShader);

    nvrhi::ComputePipelineHandle pipeline = nvrhiDevice->createComputePipeline(pipelineDesc);
    nvrhi::CommandListHandle commandList = nvrhiDevice->createCommandList();
    commandList->open();
    commandList->beginTrackingBufferState(bufA, nvrhi::ResourceStates::ShaderResource);
    commandList->beginTrackingBufferState(bufB, nvrhi::ResourceStates::ShaderResource);
    commandList->beginTrackingBufferState(bufOut, nvrhi::ResourceStates::UnorderedAccess);

    nvrhi::ComputeState computeState;
    computeState.pipeline = pipeline;
    computeState.bindings = {bindingSet};

    commandList->setComputeState(computeState);
    commandList->dispatch(elementCount, 1, 1);
    commandList->close();
    nvrhiDevice->executeCommandList(commandList);

    // -------------------------
    // 6. Read back and print result
    // -------------------------
    std::vector<float> result(elementCount);

    nvrhi::BufferDesc readbackDesc;
    readbackDesc.setByteSize(result.size() * sizeof(float))
        .setCpuAccess(nvrhi::CpuAccessMode::Read)
        .setInitialState(nvrhi::ResourceStates::CopyDest)
        .setDebugName("ReadbackBuffer");

    nvrhi::BufferHandle readbackBuffer = nvrhiDevice->createBuffer(readbackDesc);

    auto readCmd = nvrhiDevice->createCommandList();
    readCmd->open();
    readCmd->beginTrackingBufferState(bufOut, nvrhi::ResourceStates::CopySource);
    readCmd->copyBuffer(
        readbackBuffer,               // dest
        0,                            // dest offset in bytes
        bufOut,                       // src (GPU buffer)
        0,                            // src offset in bytes
        result.size() * sizeof(float) // number of bytes to copy
    );
    readCmd->close();
    nvrhiDevice->executeCommandList(readCmd);
    nvrhiDevice->waitForIdle();

    void* mappedPtr = nvrhiDevice->mapBuffer(readbackBuffer, nvrhi::CpuAccessMode::Read);
    if (mappedPtr)
    {
        std::memcpy(result.data(), mappedPtr, result.size() * sizeof(float));
        nvrhiDevice->unmapBuffer(readbackBuffer);
    }
    else
        std::cerr << "Failed to map readback buffer." << std::endl;

    std::cout << "Result: ";
    for (float val : result)
        std::cout << val << " ";
    std::cout << std::endl;
    return 0;
}
