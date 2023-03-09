#include "benzin/config/bootstrap.hpp"

#include "benzin/core/logger.hpp"

namespace benzin
{

    constexpr magic_enum::containers::array<Logger::Severity, fmt::color> g_SeverityColors
    {
        fmt::color::green,
        fmt::color::yellow,
        fmt::color::red
    };

    const auto g_StartTimePoint = std::chrono::system_clock::now().time_since_epoch();

    void Logger::LogImpl(Severity severity, std::string_view filepath, uint32_t line, std::string_view message)
    {
        const auto logTimePoint = std::chrono::system_clock::now().time_since_epoch();
        const auto passTime = std::chrono::duration_cast<std::chrono::milliseconds>(logTimePoint - g_StartTimePoint);

        const uint64_t hours = std::chrono::duration_cast<std::chrono::hours>(passTime).count();
        const uint64_t minutes = std::chrono::duration_cast<std::chrono::minutes>(passTime).count() - hours * 60;
        const uint64_t seconds = std::chrono::duration_cast<std::chrono::seconds>(passTime).count() - hours * 60 - minutes * 60;
        const uint64_t milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(passTime).count() - hours * 60 - minutes * 60 - seconds * 1000;

        const auto formatedSeverity = fmt::format("{}", fmt::styled(magic_enum::enum_name(severity), fmt::fg(g_SeverityColors[severity])));
        const auto formatedTime = fmt::format("{}:{}:{:0>2}.{:0>3}", hours, minutes, seconds, milliseconds);
        const auto formatedFilename = fmt::format("{}:{}", filepath.substr(filepath.find_last_of("\\") + 1), line);

        fmt::print(
            FMT_COMPILE("[{}][{}][{}]: {}"),
            formatedSeverity,
            formatedTime,
            formatedFilename,
            message
        );

        fmt::print("\n");
    }

} // namespace benzin
