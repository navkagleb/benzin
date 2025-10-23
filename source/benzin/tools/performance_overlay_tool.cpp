#include <benzin/config/bootstrap.hpp>
#include <benzin/tools/performance_overlay_tool.hpp>

#include <benzin/core/cmd_line_args.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics2/shader_manager.hpp>
#include <benzin/system/window.hpp>

namespace benzin
{

    PerformanceOverlayTool::PerformanceOverlayTool(const Backend& backend, const ShaderManager& shaderManager, const RenderViewport& viewport)
        : ImGuiTool{ "Debug/PerformanceOverlay" }
        , m_Backend{ backend }
        , m_ShaderManager{ shaderManager }
        , m_Viewport{ viewport }
    {
        ms_IntervalTimer->AddCallback([this](float timeInMs, uint32_t frameCount)
        {
            m_AvgDeltaTimeInMs = timeInMs / frameCount;
            m_AvgFps = 1.0f / (m_AvgDeltaTimeInMs / 1000.0f);
        });
    }

    void PerformanceOverlayTool::DrawWindow()
    {
        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;

        if (m_Location != OverlayLocation::Custom)
        {
            const float padding = 10.0f;

            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            const ImVec2 workPosition = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
            const ImVec2 workSize = viewport->WorkSize;

            ImVec2 windowPosition;
            windowPosition.x = ((uint8_t)m_Location & 1) == 0 ? workPosition.x + padding : workPosition.x + workSize.x - padding;
            windowPosition.y = ((uint8_t)m_Location & 2) == 0 ? workPosition.y + padding : workPosition.y + workSize.y - padding;

            ImVec2 windowPositionPivot;
            windowPositionPivot.x = ((uint8_t)m_Location & 1) == 0 ? 0.0f : 1.0f;
            windowPositionPivot.y = ((uint8_t)m_Location & 2) == 0 ? 0.0f : 1.0f;

            ImGui::SetNextWindowPos(windowPosition, ImGuiCond_Always, windowPositionPivot);
            ImGui::SetNextWindowViewport(viewport->ID);

            windowFlags |= ImGuiWindowFlags_NoMove;
        }

        constexpr auto backgroundColors = std::to_array(
        {
            IM_COL32(200, 50, 0, 255),
            IM_COL32(184, 100, 0, 255),
        });

        ImGui::SetNextWindowBgAlpha(0.9f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, backgroundColors[m_ShaderManager.IsEachShaderGood()]);
        ImGuiTool::DrawWindow(windowFlags);
        ImGui::PopStyleColor();
    }

    void PerformanceOverlayTool::DrawWindowContent()
    {
        const AdapterMemoryInfo adapterMemoryInfo = m_Backend.GetMainAdapterMemoryInfo();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0.0f, 0.0f });

        ImGui::FmtText("FPS: {:.1f} ({:.3f} ms)", m_AvgFps, m_AvgDeltaTimeInMs);
        ImGui::NewLine();
        ImGui::FmtText("{}", m_Backend.GetMainAdapterInfo().m_Name);
        ImGui::FmtText("Local VRAM: {:.0f} / {:.0f} mb", ToMb(adapterMemoryInfo.m_UsedLocalVramInBytes), ToMb(adapterMemoryInfo.m_LocalVramBudgetInBytes));
        ImGui::FmtText("Host VRAM: {:.0f} mb", ToMb(adapterMemoryInfo.m_UsedHostVramInBytes));
        ImGui::NewLine();
        ImGui::FmtText("Window: {} x {}", ms_Window->GetWidth(), ms_Window->GetHeight());
        ImGui::FmtText("Viewport: {} x {}", m_Viewport.GetWidth(), m_Viewport.GetHeight());

        if (CmdLineArgs::IsGpuValidationEnabled())
        {
            ImGui::Text("!!! GPU Validation ENABLED");
        }

        if (CmdLineArgs::IsSynchronizedCommandQueueValidationEnabled())
        {
            ImGui::Text("!!! Debug Sync Queue ENABLED");
        }

        ImGui::PopStyleVar();

        if (ImGui::BeginPopupContextWindow())
        {
            const auto spawnMenuItem = [&](OverlayLocation selectedLocation)
            {
                const auto name = magic_enum::enum_name(selectedLocation);
                if (ImGui::MenuItem(name.data(), nullptr, m_Location == selectedLocation))
                {
                    m_Location = selectedLocation;
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

}
