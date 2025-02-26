#include "benzin/config/bootstrap.hpp"

#include "benzin/core/logger.hpp"

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

    bool Assert(std::string_view conditionString, const std::source_location& sourceLocation, std::string_view message1, std::string_view message2)
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

        if (!message1.empty())
        {
            std::format_to(std::back_inserter(buffer), "-- Message1: {}\n", message1);
        }

        if (!message2.empty())
        {
            std::format_to(std::back_inserter(buffer), "-- Message2: {}\n", message2);
        }

        BenzinError("{}", buffer);

        if (!::IsDebuggerPresent())
        {
            ShowAbortMessageBox();
        }

        return true;
    }

}
