#pragma once
#include <slang.h>
#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <nvrhi/nvrhi.h>
#include <string>
#include <unordered_map>

#include "Utils/Logger.h"
#include "Core/Program/ReflectionInfo.h"

class Program
{
public:
    Program(
        nvrhi::IDevice* device,
        const std::string& filePath,
        const std::unordered_map<std::string, nvrhi::ShaderType>& entryPoints,
        const std::string& profile
    );

    nvrhi::ShaderHandle getShader(const std::string& entryPoint) const;
    slang::ProgramLayout* getProgramLayout() const { return mpProgramLayout; }
    const std::vector<nvrhi::ShaderHandle>& getShaders() const { return mShaders; }
    std::vector<ReflectionInfo> getReflectionInfo() const { return mReflectionInfo; }

    // Debugging utility to print reflection information
    void printReflectionInfo() const;

    bool generateBindingLayout();

private:
    // Initialize session for Slang compilation
    void initializeSession(const std::string& profile);

    void printScope(slang::VariableLayoutReflection* pScopeVarLayout, int indent) const;

    void printVarLayout(slang::VariableLayoutReflection* pVarLayout, int indent) const;

    void printTypeLayout(slang::TypeLayoutReflection* pTypeLayout, int indent) const;

    void printRelativeOffsets(slang::VariableLayoutReflection* pVarLayout, int indent) const;

    void printOffset(slang::VariableLayoutReflection* pVarLayout, slang::ParameterCategory layoutUnit, int indent) const;

    void printOffsets(slang::VariableLayoutReflection* pVarLayout, int indent) const;

    void printSize(slang::TypeLayoutReflection* pTypeLayout, slang::ParameterCategory layoutUnit, int indent) const;

    void printSizes(slang::TypeLayoutReflection* pTypeLayout, int indent) const;

    std::string printKind(slang::TypeReflection::Kind kind) const;

    std::string printLayoutUnit(slang::ParameterCategory layoutUnit) const;

    bool processParameterGroup(slang::VariableLayoutReflection* varLayout);

    bool processParameter(slang::VariableLayoutReflection* varLayout, int bindingSpaceOffset = 0, std::string prefix = "");
    Slang::ComPtr<slang::IGlobalSession> mGlobalSession;
    Slang::ComPtr<slang::ISession> mSession;
    Slang::ComPtr<slang::ICompileRequest> mCompileRequest;
    Slang::ComPtr<slang::IComponentType> mLinkedProgram;
    slang::ProgramLayout* mpProgramLayout;

    std::vector<nvrhi::ShaderHandle> mShaders;                        // All shaders
    std::unordered_map<std::string, size_t> mEntryPointToShaderIndex; // Map entry point names to shader indices
    std::vector<ReflectionInfo> mReflectionInfo;
};
