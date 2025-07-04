#pragma once
#include <slang.h>
#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <nvrhi/nvrhi.h>
#include <string>
#include <unordered_map>

#include "Utils/Logger.h"

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
    std::vector<nvrhi::BindingLayoutItem> getBindingLayoutItems() { return m_BindingLayoutItems; }
    std::unordered_map<std::string, nvrhi::BindingSetItem> getBindingSetItems() { return m_BindingSetItems; }

    // Debugging utility to print reflection information
    void printReflectionInfo() const;

    bool generateBindingLayout();

private:
    // Initialize session for Slang compilation
    void initializeSession(const std::string& profile);

    void printVariableLayout(slang::VariableLayoutReflection* varLayout, int indent) const;

    bool processParameterGroup(slang::VariableLayoutReflection* varLayout);

    bool processParameter(slang::VariableLayoutReflection* varLayout);

    Slang::ComPtr<slang::IGlobalSession> m_GlobalSession;
    Slang::ComPtr<slang::ISession> m_Session;
    Slang::ComPtr<slang::ICompileRequest> m_CompileRequest;
    Slang::ComPtr<slang::IComponentType> m_LinkedProgram;
    slang::ProgramLayout* m_ProgramLayout;

    std::vector<nvrhi::ShaderHandle> m_Shaders;                               // All shaders
    std::unordered_map<std::string, size_t> m_EntryPointToShaderIndex;        // Map entry point names to shader indices
    std::vector<nvrhi::BindingLayoutItem> m_BindingLayoutItems;               // Binding layout items
    std::unordered_map<std::string, nvrhi::BindingSetItem> m_BindingSetItems; // Binding set items
};
