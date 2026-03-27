#include <sandbox/bootstrap.hpp>

#include <sandbox/sandbox_runner.hpp>

#include <benzin/core/entry_point.hpp>

int benzin::ClientMain()
{
    // sandbox::SponzaRunner runner;
    sandbox::OccusionCullingRunner runner;
    runner.RunMainLoop();

    return 0;
}
