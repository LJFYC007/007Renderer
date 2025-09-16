#pragma once

#include <iostream>
#include <string>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <nvrhi/nvrhi.h>
#include <nvrhi/d3d12.h>
#include <nvrhi/validation.h>

#include "Pointer.h"

using Microsoft::WRL::ComPtr;

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

// Message callback class for NVRHI
class MessageCallback : public nvrhi::IMessageCallback
{
public:
    void message(nvrhi::MessageSeverity severity, const char* messageText) override;
};

class Device
{
public:
    Device();
    ~Device() { shutdown(); }

    // Initialize D3D12 and NVRHI devices
    bool initialize();
    void shutdown(); // Getters
    ComPtr<ID3D12Device> getD3D12Device() const { return mpD3d12Device; }
    ComPtr<ID3D12CommandQueue> getCommandQueue() const { return mpCommandQueue; }
    nvrhi::CommandListHandle getCommandList() const { return mCommandList; }
    nvrhi::DeviceHandle getDevice() const { return mNvrhiDevice; }

    // Check if device is valid
    bool isValid() const { return mpD3d12Device && mNvrhiDevice; }

private:
    // Helper methods
    bool createD3D12Device();
    bool createNVRHIDevice();
    bool createCommandQueue();
    ComPtr<ID3D12Device> mpD3d12Device;
    ComPtr<ID3D12CommandQueue> mpCommandQueue;
    nvrhi::CommandListHandle mCommandList;
    ComPtr<IDXGIFactory4> mpDxgiFactory;
    ComPtr<IDXGIAdapter1> mpAdapter;
    nvrhi::DeviceHandle mNvrhiDevice;
    nvrhi::CommandListParameters mCmdParams;

    ref<MessageCallback> mpMessageCallback;
    bool mIsInitialized = false;

#ifdef _DEBUG
    ID3D12Debug* mpDx12Debug = nullptr;
    ID3D12InfoQueue* mpInfoQueue = nullptr;
#endif
};
