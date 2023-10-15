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
        BENZIN_NON_CONSTRUCTABLE_IMPL(Logger)

    public:
        static void Log(LogSeverity severity, const std::source_location& sourceLocation, fmt::string_view format, fmt::format_args args)
        {
            LogImpl(severity, sourceLocation, fmt::vformat(format, args));
        }

    private:
        static void LogImpl(LogSeverity severity, const std::source_location& sourceLocation, std::string_view message);
    };

    template <typename... Args>
    struct Log
    {
        explicit Log(LogSeverity severity, fmt::format_string<Args...> format, Args&&... args, const std::source_location& sourceLocation = std::source_location::current())
        {
            Logger::Log(severity, sourceLocation, format, fmt::make_format_args(args...));
        }
    };

    template <typename... Args>
    Log(LogSeverity, std::string_view, Args&&...) -> Log<Args...>;

} // namespace benzin

#define BenzinTrace(format, ...) ::benzin::Log{ ::benzin::LogSeverity::Trace, format, __VA_ARGS__ }
#define BenzinWarning(format, ...) ::benzin::Log{ ::benzin::LogSeverity::Warning, format, __VA_ARGS__ }
#define BenzinError(format, ...) ::benzin::Log{ ::benzin::LogSeverity::Error, format, __VA_ARGS__ }
