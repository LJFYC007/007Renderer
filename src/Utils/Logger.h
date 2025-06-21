#pragma once
#include <spdlog/spdlog.h>
#include <streambuf>
#include <iostream>
#include <string>

#define LOG_INFO(...) SPDLOG_LOGGER_INFO(Logger::get(), __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(Logger::get(), "[{}:{}] {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__))
#define LOG_WARN(...) SPDLOG_LOGGER_WARN(Logger::get(), "[{}:{}] {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__))
#define LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(Logger::get(), __VA_ARGS__)

// Custom streambuf to redirect cout/cerr to spdlog
class SpdlogStreambuf : public std::streambuf
{
public:
    explicit SpdlogStreambuf(std::shared_ptr<spdlog::logger> logger, spdlog::level::level_enum level);
    ~SpdlogStreambuf() override;

protected:
    int overflow(int c) override;
    int sync() override;

private:
    std::shared_ptr<spdlog::logger> logger_;
    spdlog::level::level_enum level_;
    std::string buffer_;
};

class StreamRedirector
{
public:
    StreamRedirector(std::shared_ptr<spdlog::logger> logger);
    ~StreamRedirector();

private:
    std::unique_ptr<SpdlogStreambuf> cout_buf_;
    std::unique_ptr<SpdlogStreambuf> cerr_buf_;
    std::streambuf* orig_cout_buf_;
    std::streambuf* orig_cerr_buf_;
};

class Logger
{
public:
    static std::shared_ptr<spdlog::logger> get();
    static void init();
    static std::unique_ptr<StreamRedirector> createStreamRedirector();
};