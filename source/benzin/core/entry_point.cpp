#include "benzin/config/bootstrap.hpp"
#include "benzin/core/entry_point.hpp"

#include "benzin/core/command_line_args.hpp"

#if BENZIN_IS_PLATFORM_WIN64

namespace benzin
{

    int Main(int argc, char** argv)
    {
        CommandLineArgsInstance::Initialize(argc, argv);

        return ClientMain();
    }

} // namespace benzin

int main(int argc, char** argv)
{
    return benzin::Main(argc, argv);
}

#endif // BENZIN_IS_PLATFORM_WIN64
