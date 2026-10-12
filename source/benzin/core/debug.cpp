#include <benzin/config/bootstrap.hpp>
#include <benzin/core/debug.hpp>

#include <benzin/core/logger.hpp>

namespace benzin
{

    static void LogBufferInternal(std::span<const char> buffer)
    {
        std::fwrite(buffer.data(), 1, buffer.size(), stdout);
        std::fputc('\n', stdout);

        OutputDebugStringA(buffer.data());
    };

    //

    const std::locale& Logger::GetThoudandSeperatorApostrophe3()
    {
        struct ThoudandSeperatorApostrophe3 : std::numpunct<char>
        {
            char do_thousands_sep() const override { return '\''; }

            std::string do_grouping() const override { return "\3"; }
        };

        static const std::locale locale{ std::locale::classic(), new ThoudandSeperatorApostrophe3 };

        return locale;
    }

    void Log(LogSeverity severity, std::string_view message, const std::source_location& sourceLocation)
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

        std::array<char, 1_kb> bufferStorage;
        const std::format_to_n_result buffer = std::format_to_n(
            bufferStorage.data(),
            bufferStorage.size() - 1ull,
            "[{}:{:0>2}.{:0>3}][{:5}][{}]: {}",
            m,
            s,
            ms,
            g_Tid,
            logSeverityToChar(severity),
            message);

        BenzinAssert((size_t)buffer.size < bufferStorage.size(), "Log message is too long and has been truncated...");
        *buffer.out = '\0';

        LogBufferInternal(ToSpan(bufferStorage.data(), buffer.size + 1));
    }

    bool Ensure(
        std::string_view conditionString,
        std::span<const std::string> messages,
        const std::source_location& sourceLocation)
    {
        std::array<char, 1_kb> bufferStorage;
        std::format_to_n_result buffer = std::format_to_n(
            bufferStorage.data(),
            bufferStorage.size() - 1,
            "\n"
            "-- Assertion failed !!!\n"
            "-- {}:{}\n"
            "-- Function: {}\n"
            "-- Condition: {}\n",
            sourceLocation.file_name(), sourceLocation.line(),
            sourceLocation.function_name(),
            conditionString);

        uint32_t messageCount = 0;
        for (const std::string_view message : messages)
        {
            if (message.empty())
                continue;

            buffer = std::format_to_n(
                bufferStorage.data() + buffer.size,
                bufferStorage.size() - buffer.size - 1,
                "-- Message{}: {}\n",
                messageCount++,
                message);
        }

        BenzinError("{}", std::string_view{bufferStorage.data(), buffer.out});

        if (!::IsDebuggerPresent())
        {
            ::MessageBox(
                nullptr,
                "Assertion failed!\nClick Abort to exit",
                "Error",
                MB_OK | MB_ICONERROR);

            ::ExitProcess(0);
        }

        return true;
    }

}
