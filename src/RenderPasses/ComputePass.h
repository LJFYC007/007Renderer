#pragma once
#include <nvrhi/nvrhi.h>

#include "Core/ShaderProgram.h"

class ComputePass
{
public:
    bool initialize(
        nvrhi::IDevice* device,
        const std::string& shaderPath,
        const std::string& entryPoint,
        const std::unordered_map<std::string, nvrhi::ResourceHandle>& resourceMap
    );

    // Dispatch the compute shader with the specified number of threads
    void dispatchThreads(nvrhi::IDevice* device, uint32_t totalThreadsX, uint32_t totalThreadsY, uint32_t totalThreadsZ);

private:
    void dispatch(nvrhi::IDevice* device, uint32_t threadGroupX, uint32_t threadGroupY, uint32_t threadGroupZ);

    nvrhi::ComputePipelineHandle m_Pipeline;
    nvrhi::BindingSetHandle m_BindingSet;
    nvrhi::BindingLayoutHandle m_BindingLayout;
    nvrhi::ShaderHandle m_Shader;

    uint32_t m_WorkGroupSizeX = 1;
    uint32_t m_WorkGroupSizeY = 1;
    uint32_t m_WorkGroupSizeZ = 1;
};
