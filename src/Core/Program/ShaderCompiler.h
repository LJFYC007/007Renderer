#pragma once
#include <slang.h>
#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Process-wide owner of the Slang IGlobalSession and a cache of ISessions keyed
// by {profile, sorted defines}. Sharing sessions means a module imported by
// multiple Programs (e.g. Scene.slang) is front-end compiled only once.
//
// NOT thread-safe: Slang's loadModule/link are not reentrant, and concurrent
// callers would also race on the session cache. All shader init runs on the
// main thread today; if that ever changes, add a mutex around getSession and
// around front-end work performed on the returned session.
//
// NO runtime hot-reload: ISession::loadModule() caches module IR on first load,
// so a Program constructed after a cached session exists will reuse old IR even
// if the .slang file has been edited on disk. Restart the app to pick up shader
// edits. If hot-reload becomes a desired workflow, add file-mtime invalidation
// (walk the session's loaded modules, compare against disk, clear the session
// entry if any source is stale).
class ShaderCompiler
{
public:
    static ShaderCompiler& get();

    // Returns a session cached by {profile, sorted defines}. Non-owning — the
    // ShaderCompiler singleton outlives any caller.
    slang::ISession* getSession(const std::string& profile, const std::vector<std::pair<std::string, std::string>>& defines);

private:
    ShaderCompiler();

    struct SessionKey
    {
        std::string profile;
        std::vector<std::pair<std::string, std::string>> sortedDefines;
        bool operator==(const SessionKey& other) const { return profile == other.profile && sortedDefines == other.sortedDefines; }
    };

    struct SessionKeyHash
    {
        size_t operator()(const SessionKey& key) const;
    };

    Slang::ComPtr<slang::IGlobalSession> mGlobalSession;
    std::unordered_map<SessionKey, Slang::ComPtr<slang::ISession>, SessionKeyHash> mSessions;
};
