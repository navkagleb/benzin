#include "benzin/config/bootstrap.hpp"
#include "benzin/core/logger.hpp"

namespace benzin
{

    namespace
    {

        constexpr auto GetSeverityColors()
        {
            magic_enum::containers::array<LogSeverity, fmt::color> severityColors;
            severityColors[LogSeverity::Trace] = fmt::color::green;
            severityColors[LogSeverity::Warning] = fmt::color::yellow;
            severityColors[LogSeverity::Error] = fmt::color::red;

            return severityColors;
        }

        constexpr auto g_SeverityColors = GetSeverityColors();

        const auto g_StartTimePoint = std::chrono::system_clock::now().time_since_epoch();

        std::string GetSeverityFormat(LogSeverity severity)
        {
            return fmt::format("{}", fmt::styled(magic_enum::enum_name(severity), fmt::fg(g_SeverityColors[severity])));
        }

        std::string GetTimePointFormat()
        {
            using namespace std::chrono;

            const auto logTimePoint = system_clock::now().time_since_epoch();
            const auto passTime = duration_cast<milliseconds>(logTimePoint - g_StartTimePoint);

            const uint64_t h = duration_cast<hours>(passTime).count();
            const uint64_t m = duration_cast<minutes>(passTime).count() - h * 60;
            const uint64_t s = duration_cast<seconds>(passTime).count() - h * 60 * 60 - m * 60;
            const uint64_t ms = duration_cast<milliseconds>(passTime).count() - h * 60 * 60 * 1000 - m * 60 * 1000 - s * 1000;

            return fmt::format("{}:{}:{:0>2}.{:0>3}", h, m, s, ms);
        }

        std::string GetFileNameFormat(const std::source_location& sourceLocation)
        {
            const std::string_view filePath = sourceLocation.file_name();

            return fmt::format("{}:{}", filePath.substr(filePath.find_last_of("\\") + 1), sourceLocation.line());
        }

        std::string GetOutput(std::string_view severity, std::string_view time, std::string_view fileName, std::string_view message)
        {
            return fmt::format(FMT_COMPILE("[{}][{}][{}]: {}\n"), severity, time, fileName, message);
        }

    } // anonymous namespace

    void Logger::LogImpl(LogSeverity severity, const std::source_location& sourceLocation, std::string_view message)
    {
        const auto severityFormat = GetSeverityFormat(severity);
        const auto timePointFormat = GetTimePointFormat();
        const auto fileNameFormat = GetFileNameFormat(sourceLocation);

        const auto consoleOutput = GetOutput(severityFormat, timePointFormat, fileNameFormat, message);
        const auto ideOutput = GetOutput(magic_enum::enum_name(severity), timePointFormat, fileNameFormat, message);

        fmt::print(consoleOutput);
        OutputDebugStringA(ideOutput.c_str());
    }

} // namespace benzin
