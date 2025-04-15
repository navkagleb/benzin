#include "benzin/config/bootstrap.hpp"

namespace benzin
{

    static void ShowAbortMessageBox()
    {
        ::MessageBox(
            nullptr,
            "Assertion failed!\nClick Abort to exit",
            "Error",
            MB_OK | MB_ICONERROR
        );

        ::ExitProcess(0);
    }

    bool Assert(std::string_view conditionString, const std::source_location& sourceLocation, std::span<const std::string> messages)
    {
        std::string buffer;
        buffer.reserve(1_kb);

        std::format_to(
            std::back_inserter(buffer),
            "\n"
            "-- Assertion failed !!!\n"
            "-- {}:{}\n"
            "-- Function: {}\n"
            "-- Condition: {}\n",
            sourceLocation.file_name(), sourceLocation.line(),
            sourceLocation.function_name(),
            conditionString
        );

        uint32_t messageCount = 0;
        for (const auto& message : messages)
        {
            if (!message.empty())
            {
                std::format_to(std::back_inserter(buffer), "-- Message{}: {}\n", messageCount++, message);
            }
        }

        BenzinError("{}", buffer);

        if (!::IsDebuggerPresent())
        {
            ShowAbortMessageBox();
        }

        return true;
    }

}
