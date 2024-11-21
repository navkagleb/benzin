#include "sandbox/bootstrap.hpp"

#include <benzin/core/entry_point.hpp>

#if 1

#include "sandbox/sandbox_runner.hpp"

#include <benzin/graphics/texture.hpp>

int benzin::ClientMain()
{
    sandbox::SandboxRunner runner;
    runner.RunMainLoop();

    return 0;
}

#else

#include <benzin/core/logger.hpp>

int benzin::ClientMain()
{
    for (uint32_t i = 0; i < 4; ++i)
    {
        BenzinTrace("{} -> {:04b}", i, i);
    }

    return 0;
}
#endif
