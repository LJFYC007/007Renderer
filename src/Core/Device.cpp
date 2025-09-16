#include "Device.h"
#include "../Utils/Logger.h"

// Helper macro for checking HRESULT
#define CHECKHR(x)                           \
    if (FAILED(x))                           \
    {                                        \
        LOG_ERROR("HRESULT failed: {}", #x); \
        return false;                        \
    }

Device::Device() : mIsInitialized(false)
{
    mpMessageCallback = make_ref<MessageCallback>();
}

bool Device::initialize()
{
    if (mIsInitialized)
        return true;

    // Enable debug layer in debug builds
#ifdef _DEBUG
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&mpDx12Debug))))
        mpDx12Debug->EnableDebugLayer();
#endif

    // Create DXGI factory
    if (!SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&mpDxgiFactory))))
    {
        LOG_ERROR("Failed to create DXGI factory");
        return false;
    }

    if (!createD3D12Device())
        return false;
    if (!createCommandQueue())
        return false;
    if (!createNVRHIDevice())
        return false;
    mCommandList = mNvrhiDevice->createCommandList(mCmdParams);

    mIsInitialized = true;
    LOG_INFO("Device initialization completed successfully");
    return true;
}

void Device::shutdown()
{
    if (!mIsInitialized)
        return;

    LOG_INFO("Shutting down devices...");

    if (mNvrhiDevice)
    {
        mNvrhiDevice->waitForIdle();
        mNvrhiDevice = nullptr;
    }

    mpCommandQueue.Reset();
    mpAdapter.Reset();
    mpD3d12Device.Reset();

#ifdef _DEBUG
    if (mpDx12Debug)
    {
        mpDx12Debug->Release();
        mpDx12Debug = nullptr;
    }
#endif

    mIsInitialized = false;
    LOG_INFO("Device shutdown completed");
}

bool Device::createD3D12Device()
{
    // Try to find a hardware adapter first
    for (UINT adapterIndex = 0;; ++adapterIndex)
    {
        if (FAILED(mpDxgiFactory->EnumAdapters1(adapterIndex, &mpAdapter)))
            break;

        DXGI_ADAPTER_DESC1 desc;
        mpAdapter->GetDesc1(&desc);

        // Skip software adapters
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue; // Try to create device with this adapter
        if (SUCCEEDED(D3D12CreateDevice(mpAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&mpD3d12Device))))
        {
            // Convert wide string to regular string for logging
            std::wstring wstr(desc.Description);
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
            std::string str(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, NULL, NULL);
            LOG_INFO("Using hardware adapter: {}", str);
            return true;
        }

        mpAdapter.Reset();
    }

    // If no hardware adapter worked, try WARP (software renderer)
    LOG_WARN("No hardware adapter found, trying WARP (software renderer)...");
    if (SUCCEEDED(mpDxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&mpAdapter))))
    {
        CHECKHR(D3D12CreateDevice(mpAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&mpD3d12Device)));
        LOG_INFO("Using WARP software adapter");
        return true;
    }

    LOG_ERROR("Failed to create D3D12 device with any adapter!");
    return false;
}

bool Device::createCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    CHECKHR(mpD3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mpCommandQueue)));
    return true;
}

bool Device::createNVRHIDevice()
{
    nvrhi::d3d12::DeviceDesc deviceDesc;
    deviceDesc.pDevice = mpD3d12Device.Get();
    deviceDesc.errorCB = mpMessageCallback.get();
    deviceDesc.pGraphicsCommandQueue = mpCommandQueue.Get();

    mNvrhiDevice = nvrhi::d3d12::createDevice(deviceDesc);
    if (!mNvrhiDevice)
    {
        LOG_ERROR("Failed to create NVRHI device");
        return false;
    }
    return true;
}

void MessageCallback::message(nvrhi::MessageSeverity severity, const char* messageText)
{
    LOG_INFO("[NVRHI] {}", messageText);
}