#pragma once

namespace benzin
{

    enum class LogSeverity
    {
        Trace,
        Warning,
        Error,
    };

    void Log(
        LogSeverity severity,
        std::string_view message,
        const std::source_location& sourceLocation = std::source_location::current());

    bool Ensure(
        std::string_view conditionString,
        std::span<const std::string> messages,
        const std::source_location& sourceLocation = std::source_location::current());

}

#define BenzinLog(condition, severity, ...) \
    do \
    { \
        if (condition) [[likely]] \
        { \
            const std::string message = benzin::ArgsToFormatString(__VA_ARGS__); \
            benzin::Log(severity, message); \
        } \
    } while (0)

#define BenzinTrace(format, ...) BenzinLog(true, benzin::LogSeverity::Trace, format, __VA_ARGS__)
#define BenzinWarning(format, ...) BenzinLog(true, benzin::LogSeverity::Warning, format, __VA_ARGS__)
#define BenzinError(format, ...) BenzinLog(true, benzin::LogSeverity::Error, format, __VA_ARGS__)

#define BenzinTraceIf(condition, format, ...) BenzinLog(condition, benzin::LogSeverity::Trace, format, __VA_ARGS__)
#define BenzinWarningIf(condition, format, ...) BenzinLog(condition, benzin::LogSeverity::Warning, format, __VA_ARGS__)
#define BenzinErrorIf(condition, format, ...) BenzinLog(condition, benzin::LogSeverity::Error, format, __VA_ARGS__)

#define BenzinEnsure(condition, ...) \
    do \
    { \
        if (const bool isOk{ condition }; !isOk) \
        { \
            const std::string message = benzin::ArgsToFormatString(__VA_ARGS__); \
            if (benzin::Ensure(#condition, benzin::ToSpan(&message))) \
            { \
                __debugbreak(); \
            }\
        } \
    } while (0)

#if BENZIN_DEBUG_BUILD_ENABLED
    #define BenzinAssert(condition, ...) BenzinEnsure(condition, __VA_ARGS__)
    #define BenzinAssertExpr(expression, ...) BenzinEnsure(expression, __VA_ARGS__)
#else
    #define BenzinAssert(condition, ...)
    #define BenzinAssertExpr(expression, ...) BenzinUnused(expression)
#endif