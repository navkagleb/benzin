#pragma once

namespace benzin
{

    enum class LogSeverity
    {
        Trace,
        Warning,
        Error,
    };

    void Log(LogSeverity severity, const std::source_location& sourceLocation, std::string_view message);

}

#define BenzinLog(condition, severity, ...) \
    do \
    { \
        if (condition) [[likely]] \
        { \
            const std::string message = benzin::ArgsToFormatString(__VA_ARGS__); \
            benzin::Log(severity, std::source_location::current(), message); \
        } \
    } while (0)

#define BenzinTrace(format, ...) BenzinLog(true, benzin::LogSeverity::Trace, format, __VA_ARGS__)
#define BenzinWarning(format, ...) BenzinLog(true, benzin::LogSeverity::Warning, format, __VA_ARGS__)
#define BenzinError(format, ...) BenzinLog(true, benzin::LogSeverity::Error, format, __VA_ARGS__)

#define BenzinTraceIf(condition, format, ...) BenzinLog(condition, benzin::LogSeverity::Trace, format, __VA_ARGS__)
#define BenzinWarningIf(condition, format, ...) BenzinLog(condition, benzin::LogSeverity::Warning, format, __VA_ARGS__)
#define BenzinErrorIf(condition, format, ...) BenzinLog(condition, benzin::LogSeverity::Error, format, __VA_ARGS__)
