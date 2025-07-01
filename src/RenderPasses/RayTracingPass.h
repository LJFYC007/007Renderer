#pragma once

#include "Pass.h"

class RayTracingPass : public Pass
{
public:
    RayTracingPass(ref<Device> device, const std::string& shaderPath, const std::unordered_map<std::string, nvrhi::ShaderType>& entryPoints);

    void execute(uint32_t width, uint32_t height, uint32_t depth) override;

private:
    nvrhi::ShaderHandle m_RayGenShader;
    nvrhi::ShaderHandle m_MissShader;
    nvrhi::ShaderHandle m_ClosestHitShader;
    nvrhi::rt::ShaderTableHandle m_ShaderTable;
    nvrhi::rt::PipelineHandle m_Pipeline;
    nvrhi::rt::State m_rtState;
};
