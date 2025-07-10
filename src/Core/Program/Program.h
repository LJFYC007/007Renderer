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

    slang::ProgramLayout* getProgramLayout() const { return m_ProgramLayout; }
    const std::vector<nvrhi::ShaderHandle>& getShaders() const { return m_Shaders; }
    std::vector<ReflectionInfo> getReflectionInfo() const { return m_ReflectionInfo; }

    // Debugging utility to print reflection information
    void printReflectionInfo() const;

    bool generateBindingLayout();

private:
    // Initialize session for Slang compilation
    void initializeSession(const std::string& profile);

    void printScope(slang::VariableLayoutReflection* scopeVarLayout, int indent) const;

    void printVarLayout(slang::VariableLayoutReflection* varLayout, int indent) const;

    void printTypeLayout(slang::TypeLayoutReflection* typeLayout, int indent) const;

    void printRelativeOffsets(slang::VariableLayoutReflection* varLayout, int indent) const;

    void printOffset(slang::VariableLayoutReflection* varLayout, slang::ParameterCategory layoutUnit, int indent) const;

    void printOffsets(slang::VariableLayoutReflection* varLayout, int indent) const;

    void printSize(slang::TypeLayoutReflection* typeLayout, slang::ParameterCategory layoutUnit, int indent) const;

    void printSizes(slang::TypeLayoutReflection* typeLayout, int indent) const;

    std::string printKind(slang::TypeReflection::Kind kind) const;

    std::string printLayoutUnit(slang::ParameterCategory layoutUnit) const;

    bool processParameterGroup(slang::VariableLayoutReflection* varLayout);

    bool processParameter(slang::VariableLayoutReflection* varLayout, int bindingSpaceOffset = 0, std::string prefix = "");

    Slang::ComPtr<slang::IGlobalSession> m_GlobalSession;
    Slang::ComPtr<slang::ISession> m_Session;
    Slang::ComPtr<slang::ICompileRequest> m_CompileRequest;
    Slang::ComPtr<slang::IComponentType> m_LinkedProgram;
    slang::ProgramLayout* m_ProgramLayout;

    std::vector<nvrhi::ShaderHandle> m_Shaders;                        // All shaders
    std::unordered_map<std::string, size_t> m_EntryPointToShaderIndex; // Map entry point names to shader indices
    std::vector<ReflectionInfo> m_ReflectionInfo;
};
