#include <benzin/config/bootstrap.hpp>
#include <benzin/core/entry_point.hpp>

#include <benzin/core/cmd_line_args.hpp>

namespace benzin
{

    BOOL WINAPI Win64_ConsoleHandler(DWORD signal)
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
        BenzinEnsure(::SetConsoleCtrlHandler(Win64_ConsoleHandler, true) != 0);

        CmdLineArgs::Initialize(argc, argv);
        return ClientMain();
    }

}

int main(int argc, char** argv)
{
    return benzin::Main(argc, argv);
}
