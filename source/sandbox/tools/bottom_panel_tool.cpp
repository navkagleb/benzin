#include "sandbox/bootstrap.hpp"
#include "sandbox/tools/bottom_panel_tool.hpp"

#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/system/window.hpp>
#include <benzin/utility/time_utils.hpp>

namespace sandbox
{

    BottomPanelTool::BottomPanelTool(
        const benzin::Window& window,
        const benzin::Backend& backend,
        const benzin::Device& device,
        const benzin::SwapChain& swapChain
    )
        : ImGuiTool{ "BottomPanelTool", true }
        , m_Window{ window }
        , m_Backend{ backend }
        , m_Device{ device }
        , m_SwapChain{ swapChain }
    {}

    void BottomPanelTool::SetFrameRateStats(float frameRate, float dt)
    {
        m_FrameRate = frameRate;
        m_FrameDeltaTimeMs = dt;
    }

    void BottomPanelTool::SetRunnerTimings(const RunnerTimings& timings)
    {
        m_RunnerTimings = timings;
    }

    void BottomPanelTool::SpawnImGui()
    {
        static constexpr uint32_t rowCount = 2;
        static constexpr auto backgroundColors = std::to_array(
        {
            IM_COL32(200, 50, 0, 240),
            IM_COL32(184, 100, 0, 240),
        });

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 10.0f, 5.0f });
        ImGui::PushStyleColor(ImGuiCol_WindowBg, backgroundColors[m_Backend.GetShaderManager().IsAllShadersGood()]);

        const auto& context = *ImGui::GetCurrentContext();
        const float panelHeight =
            context.FontBaseSize * rowCount + // Base panel height
            context.Style.WindowPadding.y * 2.0f + // Add padding to top and bottom
            context.Style.ItemSpacing.y * (rowCount - 1); // Add spacing between rows

        ImGui::SetNextWindowPos(ImVec2{ 0.0f, context.IO.DisplaySize.y - panelHeight });
        ImGui::SetNextWindowSize(ImVec2{ context.IO.DisplaySize.x, panelHeight });

        static constexpr auto windowFlags = ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs;
        ImGui::Begin("BottomPanel", nullptr, windowFlags);
        {
            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor();

            const auto adapterMemoryInfo = m_Backend.GetMainAdapterMemoryInfo();

            ImGui::Text(BenzinFormatData(
                "{} | "
                "VRAM Local: {:.0f} / {:.0f} mb | "
                "VRAM NonLocal: {:.0f} / {:.0f} mb | "
                "CPU: {}, Completed GPU: {}, ActiveFrame: {}",
                m_Backend.GetMainAdapterInfo().Name,
                adapterMemoryInfo.ProcessUsedDedicatedVram.GetMb(), adapterMemoryInfo.DedicatedVramOsBudget.GetMb(),
                adapterMemoryInfo.ProcessUsedSharedRam.GetMb(), adapterMemoryInfo.SharedRamOsBudget.GetMb(),
                m_Device.GetCpuFrameIndex(), m_Device.GetCompletedGpuFrameIndex(), m_Device.GetActiveFrameIndex()
            ));

            const auto beginFrameTiming = benzin::ToFloatMs(m_RunnerTimings[+RunnerTiming::BeginFrame]);
            const auto onUpdateTiming = benzin::ToFloatMs(m_RunnerTimings[+RunnerTiming::OnUpdate]);
            const auto onRenderTiming = benzin::ToFloatMs(m_RunnerTimings[+RunnerTiming::OnRender]);
            const auto endFrameTiming = benzin::ToFloatMs(m_RunnerTimings[+RunnerTiming::EndFrame]);

            ImGui::Text(BenzinFormatData(
                "({} x {}) | "
                "FPS: {:.1f} ({:.3f} ms) | "
                "Begin: {:.3f}, OnUpdate: {:.3f}, OnRender: {:.3f}, End: {:.3f} | "
                "Present: {:06.3f}, GpuWait: {:06.3f}",
                m_Window.GetWidth(), m_Window.GetHeight(),
                m_FrameRate, m_FrameDeltaTimeMs,
                beginFrameTiming, onUpdateTiming, onRenderTiming, endFrameTiming,
                benzin::ToFloatMs(m_SwapChain.GetPresentTime()), benzin::ToFloatMs(m_SwapChain.GetGpuWaitTime())
            ));
        }
        ImGui::End();
    }

}
