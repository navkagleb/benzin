#include <benzin/config/bootstrap.hpp>
#include <benzin/core/logger.hpp>

namespace benzin
{

    const std::locale& Logger::GetThoudandSeperatorApostrophe3()
    {
        struct ThoudandSeperatorApostrophe3 : std::numpunct<char>
        {
            char do_thousands_sep() const override { return '\''; }

            std::string do_grouping() const override { return "\3"; }
        };

        static const std::locale locale{std::locale::classic(), new ThoudandSeperatorApostrophe3};

        return locale;
    }

    void Log(LogSeverity severity, const std::source_location& sourceLocation, std::string_view message)
    {
        BenzinUnused(sourceLocation);

        using Clock = std::chrono::steady_clock;

        static const Clock::time_point s_StartTimePoint = Clock::now();
        static thread_local std::thread::id g_Tid = std::this_thread::get_id();

        const auto now = Clock::now();
        const auto passMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_StartTimePoint);

        const uint64_t m = passMs.count() / 60000;
        const uint64_t s = (passMs.count() / 1000) % 60;
        const uint64_t ms = passMs.count() % 1000;

        const auto logSeverityToChar = [](LogSeverity severity)
        {
            switch (severity)
            {
                case LogSeverity::Trace: return 'T';
                case LogSeverity::Warning: return 'W';
                case LogSeverity::Error: return 'E';
            }

            BenzinEnsure(false);
            return '?';
        };

        std::array<char, 1_kb> buffer;
        const auto outBuffer = std::format_to_n(
            buffer.data(),
            buffer.size() - 1,
            "[{}:{:0>2}.{:0>3}][{:5}][{}]: {}",
            m,
            s,
            ms,
            g_Tid,
            logSeverityToChar(severity),
            message);

        buffer[outBuffer.size] = '\0';

        std::fwrite(buffer.data(), 1, outBuffer.size, stdout);
        std::fputc('\n', stdout);

        OutputDebugStringA(buffer.data());
    }

}
