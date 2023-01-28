#include "benzin/config/bootstrap.hpp"

#include "benzin/core/entry_point.hpp"

#if defined(BENZIN_PLATFORM_WINDOWS)

namespace benzin
{

    int Main(int argc, char** argv)
    {
        // TODO: Make abstraction for CommandArguments
        return ClientMain();
    }

} // namespace benzin

int main(int argc, char** argv)
{
    return benzin::Main(argc, argv);
}

#endif // defined(BENZIN_PLATFORM_WINDOWS)