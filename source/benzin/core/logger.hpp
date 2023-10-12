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
        template <typename... Args>
        static void Log(LogSeverity severity, const std::source_location& sourceLocation, std::string_view format, Args&&... args)
        {
            LogImpl(severity, sourceLocation, fmt::format(format, std::forward<Args>(args)...));
        }

    private:
        static void LogImpl(LogSeverity severity, const std::source_location& sourceLocation, std::string_view message);
    };

    template <typename... Args>
    struct Log
    {
        explicit Log(LogSeverity severity, std::string_view format, Args&&... args, const std::source_location& sourceLocation = std::source_location::current())
        {
            Logger::Log(severity, sourceLocation, format, std::forward<Args>(args)...);
        }
    };

    template <typename... Args>
    Log(LogSeverity, std::string_view, Args&&...) -> Log<Args...>;

} // namespace benzin

#define BenzinTrace(format, ...) ::benzin::Log{ ::benzin::LogSeverity::Trace, format, __VA_ARGS__ }
#define BenzinWarning(format, ...) ::benzin::Log{ ::benzin::LogSeverity::Warning, format, __VA_ARGS__ }
#define BenzinError(format, ...) ::benzin::Log{ ::benzin::LogSeverity::Error, format, __VA_ARGS__ }
