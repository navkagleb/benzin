#include "spieler/config/bootstrap.hpp"

#include "spieler/core/logger.hpp"

#include <third_party/magic_enum/magic_enum.hpp>
#include <third_party/fmt/color.h>
#include <third_party/fmt/compile.h>

namespace spieler
{

    const auto g_StartTimePoint = std::chrono::system_clock::now().time_since_epoch();

    static fmt::color GetSeverityColor(Logger::Severity severity)
    {
        switch (severity)
        {
            case Logger::Severity::Info: return fmt::color::green;
            case Logger::Severity::Warning: return fmt::color::yellow;
            case Logger::Severity::Critical: return fmt::color::red;
            default: break;
        }

        return fmt::color::white;
    }

    void Logger::LogImpl(Severity severity, std::string_view filepath, uint32_t line, std::string_view message)
    {
        const auto logTimePoint = std::chrono::system_clock::now().time_since_epoch();
        const auto passTime = std::chrono::duration_cast<std::chrono::milliseconds>(logTimePoint - g_StartTimePoint);

        const uint64_t hours = std::chrono::duration_cast<std::chrono::hours>(passTime).count();
        const uint64_t minutes = std::chrono::duration_cast<std::chrono::minutes>(passTime).count() - hours * 60;
        const uint64_t seconds = std::chrono::duration_cast<std::chrono::seconds>(passTime).count() - hours * 60 - minutes * 60;
        const uint64_t milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(passTime).count() - hours * 60 - minutes * 60 - seconds * 1000;

        const auto formatedSeverity = fmt::format("{}", fmt::styled(magic_enum::enum_name(severity), fmt::fg(GetSeverityColor(severity))));
        const auto formatedTime = fmt::format("{}:{}:{:0>2}.{:0>3}", hours, minutes, seconds, milliseconds);
        const auto formatedFilename = fmt::format("{}:{}", filepath.substr(filepath.find_last_of("\\") + 1), line);

        fmt::print(
            FMT_COMPILE("[{}][{}][{}]: {}\n"),
            formatedSeverity,
            formatedTime,
            formatedFilename,
            message
        );
    }

} // namespace spieler
