#pragma once
#include <spdlog/spdlog.h>
#include <streambuf>
#include <iostream>
#include <string>
#include <memory>

#define LOG_INFO(...) SPDLOG_LOGGER_INFO(Logger::get(), __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(Logger::get(), "[{}:{}] {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__))
#define LOG_WARN(...) SPDLOG_LOGGER_WARN(Logger::get(), "[{}:{}] {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__))
#define LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(Logger::get(), __VA_ARGS__)
#define LOG_TRACE(...) SPDLOG_LOGGER_TRACE(Logger::get(), __VA_ARGS__)

#define LOG_INFO_RETURN(...)                                                                           \
    do                                                                                                 \
    {                                                                                                  \
        SPDLOG_LOGGER_INFO(Logger::get(), "[{}:{}] {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__)); \
        return;                                                                                        \
    } while (0)
#define LOG_ERROR_RETURN(...)                                                                           \
    do                                                                                                  \
    {                                                                                                   \
        SPDLOG_LOGGER_ERROR(Logger::get(), "[{}:{}] {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__)); \
        return;                                                                                         \
    } while (0)
#define LOG_WARN_RETURN(...)                                                                           \
    do                                                                                                 \
    {                                                                                                  \
        SPDLOG_LOGGER_WARN(Logger::get(), "[{}:{}] {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__)); \
        return;                                                                                        \
    } while (0)
#define LOG_DEBUG_RETURN(...)                                                                           \
    do                                                                                                  \
    {                                                                                                   \
        SPDLOG_LOGGER_DEBUG(Logger::get(), "[{}:{}] {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__)); \
        return;                                                                                         \
    } while (0)

class Logger
{
public:
    static std::shared_ptr<spdlog::logger> get();
    static void init();
};