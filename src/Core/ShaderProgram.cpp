#include "ShaderProgram.h"
#include <iostream>

ShaderProgram::ShaderProgram()
{
    slang::createGlobalSession(m_GlobalSession.writeRef());
    slang::SessionDesc desc = {};
    m_GlobalSession->createSession(desc, m_Session.writeRef());
}

bool ShaderProgram::loadFromFile(
    nvrhi::IDevice* device,
    const std::string& filePath,
    const std::string& entryPoint,
    nvrhi::ShaderType shaderType,
    const std::string& profile
)
{
    m_Session->createCompileRequest(m_CompileRequest.writeRef());

    int targetIndex = m_CompileRequest->addCodeGenTarget(SLANG_DXIL);
    SlangProfileID profileID = m_GlobalSession->findProfile(profile.c_str());
    m_CompileRequest->setTargetProfile(targetIndex, profileID);
    m_CompileRequest->setMatrixLayoutMode(SLANG_MATRIX_LAYOUT_ROW_MAJOR);

    int tuIndex = m_CompileRequest->addTranslationUnit(SLANG_SOURCE_LANGUAGE_SLANG, "mainUnit");
    m_CompileRequest->addTranslationUnitSourceFile(tuIndex, filePath.c_str());
    int entryPointIndex = m_CompileRequest->addEntryPoint(tuIndex, entryPoint.c_str(), SLANG_STAGE_COMPUTE);

    if (m_CompileRequest->compile() != SLANG_OK)
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        m_CompileRequest->getDiagnosticOutputBlob(diagnostics.writeRef());
        std::cerr << "[Slang] Compilation failed:\n" << (const char*)diagnostics->getBufferPointer();
        return false;
    }

    Slang::ComPtr<slang::IBlob> shaderBlob;
    if (SLANG_FAILED(m_CompileRequest->getEntryPointCodeBlob(entryPointIndex, targetIndex, shaderBlob.writeRef())))
    {
        std::cerr << "[Slang] Failed to get compiled blob.\n";
        return false;
    }

    nvrhi::ShaderDesc desc;
    desc.entryName = entryPoint.c_str();
    desc.shaderType = shaderType;
    m_Shader = device->createShader(desc, shaderBlob->getBufferPointer(), shaderBlob->getBufferSize());

    return m_Shader != nullptr;
}
