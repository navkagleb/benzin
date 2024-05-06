#pragma once

#include <benzin/engine/imgui_pass.hpp>

namespace benzin
{

    class Backend;
    class Device;
    class SwapChain;
    class Window;

}

namespace sandbox
{

    enum class RunnerTiming : uint32_t
    {
        BeginFrame,
        OnUpdate,
        OnRender,
        EndFrame,
    };
    BenzinEnableUnaryPlusForEnum(RunnerTiming);

    class BottomPanelTool : public benzin::ImGuiTool
    {
    public:
        using RunnerTimings = benzin::EnumArray<std::chrono::microseconds, RunnerTiming>;

        BottomPanelTool(const benzin::Window& window, const benzin::Backend& backend, const benzin::Device& device, const benzin::SwapChain& swapChain);

        void SetFrameRateStats(float frameRate, float dt);
        void SetRunnerTimings(const RunnerTimings& timings);

    private:
        void OnImGuiRender() override;

    private:
        const benzin::Window& m_Window;
        const benzin::Backend& m_Backend;
        const benzin::Device& m_Device;
        const benzin::SwapChain& m_SwapChain;

        float m_FrameRate = 0.0f;
        float m_FrameDeltaTimeMs = 0.0f;

        RunnerTimings m_RunnerTimings{};
    };

}
