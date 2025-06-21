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
    bool loadFromFile(
        nvrhi::IDevice* device,
        const std::string& filePath,
        const std::string& entryPoint,
        nvrhi::ShaderType shaderType,
        const std::string& profile = "cs_6_2"
    );

    nvrhi::ShaderHandle getShader() const { return m_Shader; }

    slang::ProgramLayout* getProgramLayout() const { return m_ProgramLayout; }

    void printReflectionInfo() const;

    bool ShaderProgram::generateBindingLayout(
        std::vector<nvrhi::BindingLayoutItem>& outLayoutItems,
        std::vector<nvrhi::BindingSetItem>& outBindings,
        const std::unordered_map<std::string, nvrhi::BufferHandle>& bufferMap
    );

private:
    void printVariableLayout(slang::VariableLayoutReflection* varLayout, int indent) const;

    bool processParameterGroup(
        slang::VariableLayoutReflection* varLayout,
        std::vector<nvrhi::BindingLayoutItem>& outLayoutItems,
        std::vector<nvrhi::BindingSetItem>& outBindings,
        const std::unordered_map<std::string, nvrhi::BufferHandle>& bufferMap
    );

    bool processParameter(
        slang::VariableLayoutReflection* varLayout,
        std::vector<nvrhi::BindingLayoutItem>& outLayoutItems,
        std::vector<nvrhi::BindingSetItem>& outBindings,
        const std::unordered_map<std::string, nvrhi::BufferHandle>& bufferMap
    );

    Slang::ComPtr<slang::IGlobalSession> m_GlobalSession;
    Slang::ComPtr<slang::ISession> m_Session;
    Slang::ComPtr<slang::ICompileRequest> m_CompileRequest;
    Slang::ComPtr<slang::IComponentType> m_LinkedProgram;
    slang::ProgramLayout* m_ProgramLayout;

    nvrhi::ShaderHandle m_Shader;
};
