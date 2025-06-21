#include "Logger.h"

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/pattern_formatter.h>

#include <memory>

static std::shared_ptr<spdlog::logger> s_Logger;

// SpdlogStreambuf implementation
SpdlogStreambuf::SpdlogStreambuf(std::shared_ptr<spdlog::logger> logger, spdlog::level::level_enum level) : logger_(logger), level_(level)
{}

SpdlogStreambuf::~SpdlogStreambuf()
{
    sync();
}

int SpdlogStreambuf::overflow(int c)
{
    if (c != EOF)
    {
        buffer_ += static_cast<char>(c);
    }
    return c;
}

int SpdlogStreambuf::sync()
{
    if (!buffer_.empty())
    {
        // Remove trailing newline if present
        if (buffer_.back() == '\n')
        {
            buffer_.pop_back();
        }

        if (!buffer_.empty())
        {
            logger_->log(level_, buffer_);
        }
        buffer_.clear();
    }
    return 0;
}

// StreamRedirector implementation
StreamRedirector::StreamRedirector(std::shared_ptr<spdlog::logger> logger)
{
    cout_buf_ = std::make_unique<SpdlogStreambuf>(logger, spdlog::level::info);
    cerr_buf_ = std::make_unique<SpdlogStreambuf>(logger, spdlog::level::err);

    orig_cout_buf_ = std::cout.rdbuf(cout_buf_.get());
    orig_cerr_buf_ = std::cerr.rdbuf(cerr_buf_.get());
}

StreamRedirector::~StreamRedirector()
{
    std::cout.rdbuf(orig_cout_buf_);
    std::cerr.rdbuf(orig_cerr_buf_);
}

void Logger::init()
{
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/007Renderer.log", true);

    console_sink->set_pattern("[%T] [%^%l%$] %v");
    file_sink->set_pattern("[%H:%M:%S.%e] [%l] %v");
    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};

    s_Logger = std::make_shared<spdlog::logger>("AsyncLogger", sinks.begin(), sinks.end());
    spdlog::register_logger(s_Logger);
    s_Logger->set_level(spdlog::level::debug);
    s_Logger->flush_on(spdlog::level::warn);
    spdlog::set_default_logger(s_Logger);
}

std::shared_ptr<spdlog::logger> Logger::get()
{
    return s_Logger;
}

std::unique_ptr<StreamRedirector> Logger::createStreamRedirector()
{
    return std::make_unique<StreamRedirector>(s_Logger);
}