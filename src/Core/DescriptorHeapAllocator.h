#include <imgui.h>
#include <d3d12.h>

// Simple free list based allocator
struct ExampleDescriptorHeapAllocator
{
    ID3D12DescriptorHeap* mpHeap = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE mHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    D3D12_CPU_DESCRIPTOR_HANDLE mHeapStartCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE mHeapStartGpu;
    UINT mHeapHandleIncrement;
    ImVector<int> mFreeIndices;
    void Create(ID3D12Device* pDevice, ID3D12DescriptorHeap* pHeap)
    {
        IM_ASSERT(mpHeap == nullptr && mFreeIndices.empty());
        mpHeap = pHeap;
        D3D12_DESCRIPTOR_HEAP_DESC desc = pHeap->GetDesc();
        mHeapType = desc.Type;
        mHeapStartCpu = mpHeap->GetCPUDescriptorHandleForHeapStart();
        mHeapStartGpu = mpHeap->GetGPUDescriptorHandleForHeapStart();
        mHeapHandleIncrement = pDevice->GetDescriptorHandleIncrementSize(mHeapType);
        mFreeIndices.reserve((int)desc.NumDescriptors);
        for (int n = desc.NumDescriptors; n > 0; n--)
            mFreeIndices.push_back(n - 1);
    }
    void Destroy()
    {
        mpHeap = nullptr;
        mFreeIndices.clear();
    }
    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* pOutCpuDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE* pOutGpuDescHandle)
    {
        IM_ASSERT(mFreeIndices.Size > 0);
        int idx = mFreeIndices.back();
        mFreeIndices.pop_back();
        pOutCpuDescHandle->ptr = mHeapStartCpu.ptr + (idx * mHeapHandleIncrement);
        pOutGpuDescHandle->ptr = mHeapStartGpu.ptr + (idx * mHeapHandleIncrement);
    }
    void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandle)
    {
        int cpuIdx = (int)((cpuDescHandle.ptr - mHeapStartCpu.ptr) / mHeapHandleIncrement);
        int gpuIdx = (int)((gpuDescHandle.ptr - mHeapStartGpu.ptr) / mHeapHandleIncrement);
        IM_ASSERT(cpuIdx == gpuIdx);
        mFreeIndices.push_back(cpuIdx);
    }
};