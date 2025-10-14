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
    private:
        void InitScene() override;
    };

    class StanfordDragonRunner : public SandboxRunner
    {
    private:
        void InitScene() override;
    };

}
