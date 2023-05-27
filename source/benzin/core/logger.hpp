#pragma once

namespace benzin
{

    class Logger
    {
    public:
        BENZIN_NON_CONSTRUCTABLE_IMPL(Logger)

    public:
        enum class Severity
        {
            Trace,
            Warning,
            Error,
        };

    public:
        template <typename... Args>
        static void Log(Severity severity, std::string_view filepath, uint32_t line, std::string_view format, Args&&... args);

    private:
        static void LogImpl(Severity severity, std::string_view filepath, uint32_t line, std::string_view message);
    };

} // namespace benzin

#define BENZIN_TRACE(format, ...) ::benzin::Logger::Log(::benzin::Logger::Severity::Trace, __FILE__, __LINE__, format, __VA_ARGS__)
#define BENZIN_WARNING(format, ...) ::benzin::Logger::Log(::benzin::Logger::Severity::Warning, __FILE__, __LINE__, format, __VA_ARGS__)
#define BENZIN_ERROR(format, ...) ::benzin::Logger::Log(::benzin::Logger::Severity::Error, __FILE__, __LINE__, format, __VA_ARGS__)

#include "logger.inl"
