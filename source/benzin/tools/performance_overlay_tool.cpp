#include "benzin/config/bootstrap.hpp"
#include "benzin/tools/performance_overlay_tool.hpp"

#include "benzin/core/math.hpp"
#include "benzin/graphics/backend.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/swap_chain.hpp"
#include "benzin/system/window.hpp"
#include "benzin/tools/render_viewport_tool.hpp"
#include "benzin/utility/time_utils.hpp"

namespace benzin
{

    enum class OverlayLocation : int8_t
    {
        Custom = -1,
        TopLeft = 0,
        TopRight,
        BottomLeft,
        BottomRight,
    };
    BenzinEnableUnaryPlusForEnum(OverlayLocation);

    //

    PerformanceOverlayTool::PerformanceOverlayTool(
        const Window& window,
        const Device& device,
        const SwapChain& swapChain,
        const RenderViewportTool& renderViewportTool
    )
        : ImGuiTool{ "PerformanceOverlayTool", true }
        , m_Window{ window }
        , m_Device{ device }
        , m_SwapChain{ swapChain }
        , m_RenderViewportTool{ renderViewportTool }
    {}

    void PerformanceOverlayTool::SetFrameRateStats(float frameRate, float dt)
    {
        m_FrameRate = frameRate;
        m_FrameDeltaTimeMs = dt;
    }

    void PerformanceOverlayTool::SpawnImGui()
    {
        static constexpr auto backgroundColors = std::to_array(
        {
            IM_COL32(200, 50, 0, 255),
            IM_COL32(184, 100, 0, 255),
        });

        static auto location = OverlayLocation::BottomLeft;
        
        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;

        if (location != OverlayLocation::Custom)
        {
            const float padding = 10.0f;

            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            const ImVec2 workPosition = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
            const ImVec2 workSize = viewport->WorkSize;

            const ImVec2 windowPosition
            {
                IsEvenQuickly(+location) ? workPosition.x + padding : workPosition.x + workSize.x - padding,
                IsDividedBy2Quickly(+location) ? workPosition.y + padding : workPosition.y + workSize.y - padding,
            };

            const ImVec2 windowPositionPivot
            {
                IsEvenQuickly(+location) ? 0.0f : 1.0f,
                IsDividedBy2Quickly(+location) ? 0.0f : 1.0f,
            };

            ImGui::SetNextWindowPos(windowPosition, ImGuiCond_Always, windowPositionPivot);
            ImGui::SetNextWindowViewport(viewport->ID);

            windowFlags |= ImGuiWindowFlags_NoMove;
        }

        const auto& backend = m_Device.GetBackend();

        ImGui::SetNextWindowBgAlpha(0.7f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, backgroundColors[backend.GetShaderManager().IsAllShadersGood()]);

        if (ImGui::Begin(m_Name.data(), &m_IsVisible, windowFlags))
        {
            const auto adapterMemoryInfo = backend.GetMainAdapterMemoryInfo();

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0.0f, 0.0f });

            ImGui::Text(BenzinFormatData("Window: {} x {}", m_Window.GetWidth(), m_Window.GetHeight()));
            ImGui::Text(BenzinFormatData("Viewport: {} x {} {}", m_RenderViewportTool.GetWidth(), m_RenderViewportTool.GetHeight(), m_RenderViewportTool.IsValidForRendering() ? '+' : '-'));
            ImGui::Text(BenzinFormatData("{}", backend.GetMainAdapterInfo().Name));
            ImGui::Text(BenzinFormatData("Fps: {:.1f} ({:.3f} ms)", m_FrameRate, m_FrameDeltaTimeMs));
            ImGui::Text(BenzinFormatData("Present: {:06.3f}, GpuWait: {:06.3f}", ToFloatMs(m_SwapChain.GetPresentTime()), ToFloatMs(m_SwapChain.GetGpuWaitTime())));
            ImGui::Text(BenzinFormatData("Cpu: {}, Gpu: {}, Frame: {}", m_Device.GetCpuFrameIndex(), m_Device.GetCompletedGpuFrameIndex(), m_Device.GetActiveFrameIndex()));
            ImGui::Text(BenzinFormatData("Vram Local: {:.0f} / {:.0f} mb", adapterMemoryInfo.ProcessUsedDedicatedVram.GetMb(), adapterMemoryInfo.DedicatedVramOsBudget.GetMb()));
            ImGui::Text(BenzinFormatData("Vram NonLocal: {:.0f} / {:.0f} mb", adapterMemoryInfo.ProcessUsedSharedRam.GetMb(), adapterMemoryInfo.SharedRamOsBudget.GetMb()));

            ImGui::PopStyleVar();

            if (ImGui::BeginPopupContextWindow())
            {
                const auto spawnMenuItem = [&](OverlayLocation selectedLocation)
                {
                    const auto name = magic_enum::enum_name(selectedLocation);
                    if (ImGui::MenuItem(name.data(), nullptr, location == selectedLocation))
                    {
                        location = selectedLocation;
                    }
                };

                spawnMenuItem(OverlayLocation::Custom);
                spawnMenuItem(OverlayLocation::TopLeft);
                spawnMenuItem(OverlayLocation::TopRight);
                spawnMenuItem(OverlayLocation::BottomLeft);
                spawnMenuItem(OverlayLocation::BottomRight);

                if (ImGui::MenuItem("Close"))
                {
                    m_IsVisible = false;
                }

                ImGui::EndPopup();
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();
    }

}
