#pragma once

namespace benzin
{

    enum class LogSeverity
    {
        Trace,
        Warning,
        Error,
    };

    class Logger
    {
    public:
        template <typename... Args>
        friend struct Log;

        BenzinDefineNonConstructable(Logger);

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

#define BenzinTrace(format, ...) ::benzin::Log{ ::benzin::LogSeverity::Trace, format, __VA_ARGS__ }
#define BenzinWarning(format, ...) ::benzin::Log{ ::benzin::LogSeverity::Warning, format, __VA_ARGS__ }
#define BenzinError(format, ...) ::benzin::Log{ ::benzin::LogSeverity::Error, format, __VA_ARGS__ }

#define BenzinTraceIf(condition, format, ...) if (condition) { BenzinTrace(format, __VA_ARGS__); }
#define BenzinWarningIf(condition, format, ...) if (condition) { BenzinWarning(format, __VA_ARGS__); }
#define BenzinErrorIf(condition, format, ...) if (condition) { BenzinError(format, __VA_ARGS__); }
