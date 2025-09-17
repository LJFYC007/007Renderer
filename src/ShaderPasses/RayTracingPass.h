#pragma once

#include "Pass.h"

class RayTracingPass : public Pass
{
public:
    RayTracingPass(ref<Device> pDevice, const std::string& shaderPath, const std::unordered_map<std::string, nvrhi::ShaderType>& entryPoints);

    void execute(uint32_t width, uint32_t height, uint32_t depth) override;

private:
    std::string getLatestLibVersion();

    nvrhi::ShaderHandle mRayGenShader;
    nvrhi::ShaderHandle mMissShader;
    nvrhi::ShaderHandle mClosestHitShader;
    nvrhi::rt::ShaderTableHandle mShaderTable;
    nvrhi::rt::PipelineHandle mPipeline;
};
