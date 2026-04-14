#pragma once

#include "Pass.h"
#include <utility>
#include <vector>

class RayTracingPass : public Pass
{
public:
    RayTracingPass(
        ref<Device> pDevice,
        const std::string& shaderPath,
        const std::vector<std::pair<std::string, nvrhi::ShaderType>>& entryPoints,
        const std::vector<std::pair<std::string, std::string>>& defines = {}
    );

    void execute(uint32_t width, uint32_t height, uint32_t depth) override;

private:
    std::string getLatestLibVersion();

    nvrhi::ShaderHandle mRayGenShader;
    std::vector<nvrhi::ShaderHandle> mMissShaders;
    nvrhi::ShaderHandle mClosestHitShader;
    nvrhi::rt::ShaderTableHandle mShaderTable;
    nvrhi::rt::PipelineHandle mPipeline;
};
