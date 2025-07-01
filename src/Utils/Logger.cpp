#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/pattern_formatter.h>

#include "Logger.h"

static ref<spdlog::logger> s_Logger;

void Logger::init()
{
    spdlog::init_thread_pool(8192, 1);
    auto console_sink = make_ref<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = make_ref<spdlog::sinks::basic_file_sink_mt>(std::string(PROJECT_LOG_DIR) + "/007Renderer.log", true);

    console_sink->set_pattern("[%T] [%^%l%$] %v");
    file_sink->set_pattern("[%H:%M:%S.%e] [%l] %v");
    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};

    s_Logger = make_ref<spdlog::async_logger>("loggername", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);

    spdlog::register_logger(s_Logger);
    s_Logger->set_level(spdlog::level::debug);
    s_Logger->flush_on(spdlog::level::warn);
    spdlog::set_default_logger(s_Logger);
}

ref<spdlog::logger> Logger::get()
{
    return s_Logger;
}