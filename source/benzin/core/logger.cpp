#include "benzin/config/bootstrap.hpp"

#include "benzin/core/logger.hpp"

namespace benzin
{

    namespace
    {

        constexpr magic_enum::containers::array<Logger::Severity, fmt::color> g_SeverityColors
        {
            fmt::color::green,
            fmt::color::yellow,
            fmt::color::red
        };

        const auto g_StartTimePoint = std::chrono::system_clock::now().time_since_epoch();

        std::string GetOutput(std::string_view severity, std::string_view time, std::string_view fileName, std::string_view message)
        {
            return fmt::format(FMT_COMPILE("[{}][{}][{}]: {}\n"), severity, time, fileName, message);
        }

    } // anonymous namespace

    void Logger::LogImpl(Severity severity, std::string_view filepath, uint32_t line, std::string_view message)
    {
        using std::chrono::duration_cast;
        using std::chrono::hours;
        using std::chrono::minutes;
        using std::chrono::seconds;
        using std::chrono::milliseconds;

        const auto logTimePoint = std::chrono::system_clock::now().time_since_epoch();
        const auto passTime = duration_cast<milliseconds>(logTimePoint - g_StartTimePoint);

        const uint64_t h = duration_cast<hours>(passTime).count();
        const uint64_t m = duration_cast<minutes>(passTime).count() - h * 60;
        const uint64_t s = duration_cast<seconds>(passTime).count() - h * 60 * 60 - m * 60;
        const uint64_t ms = duration_cast<milliseconds>(passTime).count() - h * 60 * 60 * 1000 - m * 60 * 1000 - s * 1000;

        const auto fmtSeverity = fmt::format("{}", fmt::styled(magic_enum::enum_name(severity), fmt::fg(g_SeverityColors[severity])));
        const auto fmtTime = fmt::format("{}:{}:{:0>2}.{:0>3}", h, m, s, ms);
        const auto fmtFileName = fmt::format("{}:{}", filepath.substr(filepath.find_last_of("\\") + 1), line);

        const auto consoleOutput = GetOutput(fmtSeverity, fmtTime, fmtFileName, message);
        const auto ideOutput = GetOutput(magic_enum::enum_name(severity), fmtTime, fmtFileName, message);

        fmt::print(consoleOutput);
        OutputDebugString(ideOutput.c_str());
    }

} // namespace benzin
