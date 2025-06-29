#pragma once
#include <nvrhi/nvrhi.h>
#include <vector>

#include "Core/ShaderProgram.h"
#include "Core/Device.h"

class RayTracingPass
{
public:
    bool initialize(
        Device& device,
        const std::string& shaderPath,
        const std::unordered_map<std::string, nvrhi::ShaderType>& entryPoints,
        const std::unordered_map<std::string, nvrhi::ResourceHandle>& resourceMap,
        const std::unordered_map<std::string, nvrhi::rt::AccelStructHandle>& accelStructMap = {}
    );

    void dispatch(Device& device, uint32_t width, uint32_t height, uint32_t depth = 1);

private:
    nvrhi::rt::PipelineHandle m_Pipeline;
    nvrhi::BindingSetHandle m_BindingSet;
    nvrhi::BindingLayoutHandle m_BindingLayout;
    nvrhi::ShaderHandle m_RayGenShader;
    nvrhi::ShaderHandle m_MissShader;
    nvrhi::ShaderHandle m_ClosestHitShader;
    nvrhi::rt::State m_rtState;
    nvrhi::rt::ShaderTableHandle m_ShaderTable;
};
