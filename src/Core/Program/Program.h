#pragma once
#include <slang.h>
#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <nvrhi/nvrhi.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Utils/Logger.h"
#include "Core/Program/ReflectionInfo.h"

class Program
{
public:
    Program(
        nvrhi::IDevice* device,
        const std::string& filePath,
        const std::unordered_map<std::string, nvrhi::ShaderType>& entryPoints,
        const std::string& profile,
        const std::vector<std::pair<std::string, std::string>>& defines = {}
    );

    nvrhi::ShaderHandle getShader(const std::string& entryPoint) const;
    slang::ProgramLayout* getProgramLayout() const { return mpProgramLayout; }
    const std::vector<nvrhi::ShaderHandle>& getShaders() const { return mShaders; }
    const std::vector<ReflectionInfo>& getReflectionInfo() const { return mReflectionInfo; }

    // Debugging utility to print reflection information
    void printReflectionInfo() const;

private:
    Slang::ComPtr<slang::IComponentType> mLinkedProgram;
    slang::ProgramLayout* mpProgramLayout;

    std::vector<nvrhi::ShaderHandle> mShaders;                        // All shaders
    std::unordered_map<std::string, size_t> mEntryPointToShaderIndex; // Map entry point names to shader indices
    std::vector<ReflectionInfo> mReflectionInfo;
};
