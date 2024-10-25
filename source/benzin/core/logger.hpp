#pragma once

#include "benzin/core/enum_flags.hpp"

namespace benzin
{

    enum class LogSeverity
    {
        Trace,
        Warning,
        Error,
    };

    enum class LogOptionFlag
    {
        Time = ToBit(0),
        ThreadId = ToBit(1),
        FileName = ToBit(2),
        All = Time | ThreadId | FileName,
    };
    BenzinEnableFlagsForBitEnum(LogOptionFlag);

    class Logger
    {
    public:
        template <typename... Args>
        friend struct Log;

        static constexpr std::string_view s_LineSeparator = "----------------------------------------------";

        static void Initialize(LogOptionFlags logOptionFlags = LogOptionFlag::All);

    private:
        static void LogImpl(LogSeverity severity, const std::source_location& sourceLocation, std::string_view message);
    };

    template <typename... Args>
    struct Log
    {
        explicit Log(LogSeverity severity, std::format_string<Args...> format, Args&&... args, const std::source_location& sourceLocation = std::source_location::current())
        {
            Logger::LogImpl(severity, sourceLocation, std::format(format, std::forward<Args>(args)...));
        }
    };

    template <typename... Args>
    Log(LogSeverity, std::format_string<Args...>, Args&&...) -> Log<Args...>;

} // namespace benzin

#define BenzinTrace(format, ...) benzin::Log{ benzin::LogSeverity::Trace, format, __VA_ARGS__ }
#define BenzinWarning(format, ...) benzin::Log{ benzin::LogSeverity::Warning, format, __VA_ARGS__ }
#define BenzinError(format, ...) benzin::Log{ benzin::LogSeverity::Error, format, __VA_ARGS__ }

#define BenzinTraceIf(condition, format, ...) if (condition) { BenzinTrace(format, __VA_ARGS__); }
#define BenzinWarningIf(condition, format, ...) if (condition) { BenzinWarning(format, __VA_ARGS__); }
#define BenzinErrorIf(condition, format, ...) if (condition) { BenzinError(format, __VA_ARGS__); }
