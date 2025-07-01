#pragma once
#include "Pass.h"

class ComputePass : public Pass
{
public:
    ComputePass(Device& device, const std::string& shaderPath, const std::string& entryPoint);

    void execute(uint32_t width, uint32_t height, uint32_t depth) override;

private:
    void dispatch(uint32_t width, uint32_t height, uint32_t depth);

    nvrhi::ShaderHandle m_Shader;
    nvrhi::ComputePipelineHandle m_Pipeline;
    uint32_t m_WorkGroupSizeX = 1;
    uint32_t m_WorkGroupSizeY = 1;
    uint32_t m_WorkGroupSizeZ = 1;
};
