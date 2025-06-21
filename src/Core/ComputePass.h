#pragma once
#include <nvrhi/nvrhi.h>

#include "ShaderProgram.h"

class ComputePass
{
public:
    bool initialize(
        nvrhi::IDevice* device,
        const std::string& shaderPath,
        const std::string& entryPoint,
        const std::unordered_map<std::string, nvrhi::BufferHandle>& bufferMap
    );

    void dispatch(nvrhi::IDevice* device, uint32_t threadX = 1, uint32_t threadY = 1, uint32_t threadZ = 1);

private:
    nvrhi::ComputePipelineHandle m_Pipeline;
    nvrhi::BindingSetHandle m_BindingSet;
    nvrhi::BindingLayoutHandle m_BindingLayout;
    nvrhi::ShaderHandle m_Shader;
};
