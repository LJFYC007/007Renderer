#pragma once
#include "Pass.h"

class ComputePass : public Pass
{
public:
    ComputePass(ref<Device> pDevice, const std::string& shaderPath, const std::string& entryPoint);

    void execute(uint32_t width, uint32_t height, uint32_t depth) override;

private:
    void dispatch(uint32_t width, uint32_t height, uint32_t depth);

    nvrhi::ShaderHandle mShader;
    nvrhi::ComputePipelineHandle mPipeline;
    uint32_t mWorkGroupSizeX = 1;
    uint32_t mWorkGroupSizeY = 1;
    uint32_t mWorkGroupSizeZ = 1;
};
