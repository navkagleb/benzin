#include "benzin/config/bootstrap.hpp"
#include "benzin/core/entry_point.hpp"

#include "benzin/core/command_line_args.hpp"

#if BENZIN_IS_PLATFORM_WIN64

namespace benzin
{

    BOOL WINAPI Wint64_ConsoleHandler(DWORD signal)
    {
        // Console close event should work like 'Stop Debugging'
        // It's unplanned application closure

        if (signal == CTRL_CLOSE_EVENT)
        {
            BenzinTrace("Console window is closing");
            return true;
        }

        return false;
    }

    int Main(int argc, char** argv)
    {
        BenzinAssert(::SetConsoleCtrlHandler(Wint64_ConsoleHandler, true) != 0);

        LoggerInstance::Initialize();
        AsserterInstance::Initialize();
        CommandLineArgsInstance::Initialize(argc, argv);

        return ClientMain();
    }

} // namespace benzin

int main(int argc, char** argv)
{
    return benzin::Main(argc, argv);
}

#endif // BENZIN_IS_PLATFORM_WIN64
