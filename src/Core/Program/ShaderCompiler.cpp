#include "ShaderCompiler.h"
#include "Utils/Logger.h"
#include <nvrhi/nvrhi.h>
#include <algorithm>

ShaderCompiler& ShaderCompiler::get()
{
    static ShaderCompiler sInstance;
    return sInstance;
}

ShaderCompiler::ShaderCompiler()
{
    slang::createGlobalSession(mGlobalSession.writeRef());
}

size_t ShaderCompiler::SessionKeyHash::operator()(const SessionKey& key) const
{
    size_t h = 0;
    nvrhi::hash_combine(h, key.profile);
    for (const auto& [name, value] : key.sortedDefines)
    {
        nvrhi::hash_combine(h, name);
        nvrhi::hash_combine(h, value);
    }
    return h;
}

slang::ISession* ShaderCompiler::getSession(const std::string& profile, const std::vector<std::pair<std::string, std::string>>& defines)
{
    SessionKey key;
    key.profile = profile;
    key.sortedDefines = defines;
    std::sort(key.sortedDefines.begin(), key.sortedDefines.end());

    if (auto it = mSessions.find(key); it != mSessions.end())
        return it->second;

    // Target-level options: affect code generation (debug info, optimization level).
    std::vector<slang::CompilerOptionEntry> targetOptions;
#ifdef _DEBUG
    LOG_INFO("[ShaderCompiler] Creating DEBUG session for profile: {}", profile);
    targetOptions.push_back(
        {slang::CompilerOptionName::DebugInformation, {slang::CompilerOptionValueKind::Int, SLANG_DEBUG_INFO_LEVEL_MAXIMAL, 0, nullptr, nullptr}}
    );
    targetOptions.push_back(
        {slang::CompilerOptionName::Optimization, {slang::CompilerOptionValueKind::Int, SLANG_OPTIMIZATION_LEVEL_NONE, 0, nullptr, nullptr}}
    );
#else
    LOG_INFO("[ShaderCompiler] Creating RELEASE session for profile: {}", profile);
    targetOptions.push_back(
        {slang::CompilerOptionName::DebugInformation, {slang::CompilerOptionValueKind::Int, SLANG_DEBUG_INFO_LEVEL_MINIMAL, 0, nullptr, nullptr}}
    );
    targetOptions.push_back(
        {slang::CompilerOptionName::Optimization, {slang::CompilerOptionValueKind::Int, SLANG_OPTIMIZATION_LEVEL_HIGH, 0, nullptr, nullptr}}
    );
#endif

    // Session-level options: preprocessor macros must live here so they affect module
    // loading / #ifdef evaluation across all imported modules, not just code generation.
    std::vector<slang::CompilerOptionEntry> sessionOptions;
    for (const auto& [name, value] : key.sortedDefines)
    {
        slang::CompilerOptionEntry entry{};
        entry.name = slang::CompilerOptionName::MacroDefine;
        entry.value.kind = slang::CompilerOptionValueKind::String;
        entry.value.stringValue0 = name.c_str();
        entry.value.stringValue1 = value.c_str();
        sessionOptions.push_back(entry);
    }

    slang::TargetDesc targetDesc = {};
    targetDesc.format = SLANG_DXIL;
    targetDesc.profile = mGlobalSession->findProfile(profile.c_str());
    targetDesc.compilerOptionEntries = targetOptions.data();
    targetDesc.compilerOptionEntryCount = static_cast<uint32_t>(targetOptions.size());

    const char* searchPaths[] = {PROJECT_SHADER_DIR, PROJECT_SRC_DIR};

    slang::SessionDesc sessionDesc = {};
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;
    sessionDesc.searchPaths = searchPaths;
    sessionDesc.searchPathCount = 2;
    sessionDesc.compilerOptionEntries = sessionOptions.data();
    sessionDesc.compilerOptionEntryCount = static_cast<uint32_t>(sessionOptions.size());

    Slang::ComPtr<slang::ISession> pSession;
    auto result = mGlobalSession->createSession(sessionDesc, pSession.writeRef());
    if (SLANG_FAILED(result))
    {
        LOG_ERROR("[ShaderCompiler] Failed to create session with result: {}", result);
        return nullptr;
    }

    auto [it, _] = mSessions.emplace(std::move(key), pSession);
    return it->second;
}
