#include "benzin/config/bootstrap.hpp"
#include "benzin/core/logger.hpp"

namespace benzin
{

    static const auto g_StartTimePoint = std::chrono::system_clock::now().time_since_epoch();

    static LogOptionFlags g_LogOptionFlags;

    static std::string GetTimePointFormat()
    {
        using namespace std::chrono;

        const auto logTimePoint = system_clock::now().time_since_epoch();
        const auto passTime = duration_cast<milliseconds>(logTimePoint - g_StartTimePoint);

        const uint64_t h = duration_cast<hours>(passTime).count();
        const uint64_t m = duration_cast<minutes>(passTime).count() - h * 60;
        const uint64_t s = duration_cast<seconds>(passTime).count() - h * 60 * 60 - m * 60;
        const uint64_t ms = duration_cast<milliseconds>(passTime).count() - h * 60 * 60 * 1000 - m * 60 * 1000 - s * 1000;

        return std::format("{}:{}:{:0>2}.{:0>3}", h, m, s, ms);
    }

    static std::string GetFileNameFormat(const std::source_location& sourceLocation)
    {
        const std::string_view filePath = sourceLocation.file_name();

        return std::format("{}:{}", filePath.substr(filePath.find_last_of("\\") + 1), sourceLocation.line());
    }

    static std::string GetOutput(LogSeverity severity, const std::source_location& sourceLocation, std::string_view message)
    {
        std::string logOptions;
        logOptions.reserve(256);

        const auto pushLogOptionToBuffer = [&logOptions](const auto& option)
        {
            std::format_to(std::back_inserter(logOptions), "[{}]", option);
        };

        if (g_LogOptionFlags.IsSet(LogOptionFlag::Time))
        {
            pushLogOptionToBuffer(GetTimePointFormat());
        }

        if (g_LogOptionFlags.IsSet(LogOptionFlag::ThreadId))
        {
            pushLogOptionToBuffer(std::this_thread::get_id());
        }

        if (g_LogOptionFlags.IsSet(LogOptionFlag::FileName))
        {
            pushLogOptionToBuffer(GetFileNameFormat(sourceLocation));
        }

        return std::format("{}[{}]: {}\n", logOptions, magic_enum::enum_name(severity), message);
    }

    //

    void Logger::Initialize(LogOptionFlags logOptionFlags)
    {
        g_LogOptionFlags = logOptionFlags;
    }

    void Log(LogSeverity severity, const std::source_location& sourceLocation, std::string_view message)
    {
        const auto output = GetOutput(severity, sourceLocation, message);

        std::print("{}", output);
        OutputDebugStringA(output.c_str());
    }

}
