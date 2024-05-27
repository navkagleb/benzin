#include "sandbox/bootstrap.hpp"

#include <benzin/core/entry_point.hpp>

#include "sandbox/sandbox_runner.hpp"

int benzin::ClientMain()
{
    sandbox::SandboxRunner runner;
    runner.RunMainLoop();

    return 0;
}
