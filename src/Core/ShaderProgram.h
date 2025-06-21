#pragma once
#include <slang.h>
#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <nvrhi/nvrhi.h>
#include <string>

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

private:
    Slang::ComPtr<slang::IGlobalSession> m_GlobalSession;
    Slang::ComPtr<slang::ISession> m_Session;
    Slang::ComPtr<slang::ICompileRequest> m_CompileRequest;

    nvrhi::ShaderHandle m_Shader;
};
