#include "Environment.h"

#include <iostream>

ref<Device> BasicTestEnvironment::sDevice = nullptr;
ref<spdlog::sinks::ringbuffer_sink_mt> BasicTestEnvironment::sLogSink = nullptr;

namespace
{
// On test failure, flush the async logger and emit everything the ringbuffer sink captured
// during the run to stderr. Keeps CI green runs quiet while making failures self-contained.
class FailureLogListener : public ::testing::EmptyTestEventListener
{
public:
    void OnTestEnd(const ::testing::TestInfo& info) override
    {
        if (info.result() == nullptr || !info.result()->Failed())
            return;
        auto sink = BasicTestEnvironment::getLogSink();
        if (!sink)
            return;
        Logger::get()->flush();
        auto lines = sink->last_formatted();
        if (lines.empty())
            return;
        std::cerr << "---- captured log (" << info.test_suite_name() << "." << info.name() << ") ----\n";
        for (const auto& line : lines)
            std::cerr << line;
        std::cerr << "---- end captured log ----" << std::endl;
    }
};
} // namespace

void BasicTestEnvironment::SetUp()
{
    Logger::init();

    // Silence the console + file sinks that Logger::init() attaches, but keep the logger at
    // debug so a ringbuffer sink can still capture everything for failure diagnostics.
    for (auto& existing : Logger::get()->sinks())
        existing->set_level(spdlog::level::off);

    sLogSink = make_ref<spdlog::sinks::ringbuffer_sink_mt>(2048);
    sLogSink->set_level(spdlog::level::debug);
    Logger::get()->sinks().push_back(sLogSink);
    Logger::get()->set_level(spdlog::level::debug);

    ::testing::UnitTest::GetInstance()->listeners().Append(new FailureLogListener);

    sDevice = make_ref<Device>();
    if (!sDevice->initialize())
        FAIL() << "Failed to initialize device for tests";
    ImGui::CreateContext();
    gReadbackHeap = make_ref<ReadbackHeap>(sDevice);
}

void BasicTestEnvironment::TearDown()
{
    sDevice->getDevice()->waitForIdle();
    gReadbackHeap.reset();
    ImGui::DestroyContext();
    sDevice.reset();
    sLogSink.reset();
    spdlog::shutdown();
}

::testing::Environment* const basicEnv = ::testing::AddGlobalTestEnvironment(new BasicTestEnvironment);
