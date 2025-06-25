#pragma once
#include <slang.h>
#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <nvrhi/nvrhi.h>
#include <string>
#include <unordered_map>

#include "Utils/Logger.h"

class ShaderProgram
{
public:
    ShaderProgram();
    bool loadFromFile(nvrhi::IDevice* device, const std::string& filePath, const std::vector<std::string>& entryPoints, nvrhi::ShaderType shaderType);

    nvrhi::ShaderHandle getShader(const std::string& entryPoint) const;
    const std::vector<nvrhi::ShaderHandle>& getAllShaders() const { return m_Shaders; }

    slang::ProgramLayout* getProgramLayout() const { return m_ProgramLayout; }

    void printReflectionInfo() const;
    bool generateBindingLayout(
        std::vector<nvrhi::BindingLayoutItem>& outLayoutItems,
        std::vector<nvrhi::BindingSetItem>& outBindings,
        const std::unordered_map<std::string, nvrhi::RefCountPtr<nvrhi::IResource>>& resourceMap
    );

private:
    // Initialize with specific shader type support
    void initializeForShaderType(nvrhi::ShaderType primaryShaderType);

    void printVariableLayout(slang::VariableLayoutReflection* varLayout, int indent) const;

    bool processParameterGroup(
        slang::VariableLayoutReflection* varLayout,
        std::vector<nvrhi::BindingLayoutItem>& outLayoutItems,
        std::vector<nvrhi::BindingSetItem>& outBindings,
        const std::unordered_map<std::string, nvrhi::RefCountPtr<nvrhi::IResource>>& resourceMap
    );

    bool processParameter(
        slang::VariableLayoutReflection* varLayout,
        std::vector<nvrhi::BindingLayoutItem>& outLayoutItems,
        std::vector<nvrhi::BindingSetItem>& outBindings,
        const std::unordered_map<std::string, nvrhi::RefCountPtr<nvrhi::IResource>>& resourceMap
    );

    Slang::ComPtr<slang::IGlobalSession> m_GlobalSession;
    Slang::ComPtr<slang::ISession> m_Session;
    Slang::ComPtr<slang::ICompileRequest> m_CompileRequest;
    Slang::ComPtr<slang::IComponentType> m_LinkedProgram;
    slang::ProgramLayout* m_ProgramLayout;

    std::vector<nvrhi::ShaderHandle> m_Shaders;                        // All shaders
    std::unordered_map<std::string, size_t> m_EntryPointToShaderIndex; // Map entry point names to shader indices
};
