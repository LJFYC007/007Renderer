#pragma once
#include <spdlog/spdlog.h>
#include <streambuf>
#include <iostream>
#include <stdexcept>
#include <string>

#include "Core/Pointer.h"

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

// Log an error and throw std::runtime_error with the same message.
// Use inside constructors to signal unrecoverable init failure — a bare `return;`
// would leave the object half-constructed for the caller to dereference.
#define LOG_ERROR_THROW(...)                                                        \
    do                                                                              \
    {                                                                               \
        std::string _msg = fmt::format(__VA_ARGS__);                                \
        SPDLOG_LOGGER_ERROR(Logger::get(), "[{}:{}] {}", __FILE__, __LINE__, _msg); \
        throw std::runtime_error(_msg);                                             \
    } while (0)

class Logger
{
public:
    static ref<spdlog::logger> get();
    static void init();
};