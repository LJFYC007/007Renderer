#pragma once

#include <iostream>
#include <string>
#include <memory>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <nvrhi/nvrhi.h>
#include <nvrhi/d3d12.h>

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
    void shutdown();

    // Getters
    ComPtr<ID3D12Device> getD3D12Device() const { return m_d3d12Device; }
    ComPtr<ID3D12CommandQueue> getCommandQueue() const { return m_commandQueue; }
    nvrhi::DeviceHandle getDevice() const { return m_nvrhiDevice; }

    // Check if device is valid
    bool isValid() const { return m_d3d12Device && m_nvrhiDevice; }

private:
    // Helper methods
    bool createD3D12Device();
    bool createNVRHIDevice();
    bool createCommandQueue();

    ComPtr<ID3D12Device> m_d3d12Device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<IDXGIFactory4> m_dxgiFactory;
    ComPtr<IDXGIAdapter1> m_adapter;
    nvrhi::DeviceHandle m_nvrhiDevice;

    std::unique_ptr<MessageCallback> m_messageCallback;
    bool m_isInitialized = false;

#ifdef _DEBUG
    ID3D12Debug* m_pdx12Debug = nullptr;
#endif
};
