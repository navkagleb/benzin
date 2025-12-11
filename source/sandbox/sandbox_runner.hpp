#pragma once

#include <sandbox/runner.hpp>

namespace sandbox
{

    class SandboxRunner : public Runner
    {
    public:
        ~SandboxRunner() override;

        void InitRenderPasses() override;
        void InitTools() override;
    };

    class SponzaRunner : public SandboxRunner
    {
    public:
        void InitScene() override;
    };

    class OccusionCullingRunner : public SandboxRunner
    {
    public:
        void InitScene() override;
    };

}
