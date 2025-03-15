#include "benzin/config/bootstrap.hpp"
#include "benzin/tools/performance_overlay_tool.hpp"

#include "benzin/core/command_line_args.hpp"
#include "benzin/core/math.hpp"
#include "benzin/graphics/backend.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/swap_chain.hpp"
#include "benzin/graphics2/shader_manager.hpp"
#include "benzin/system/window.hpp"
#include "benzin/tools/render_viewport_tool.hpp"
#include "benzin/utility/time_utils.hpp"

BenzinEnableUnaryPlusForEnum(benzin::PerformanceOverlayTool::OverlayLocation);

namespace benzin
{

    PerformanceOverlayTool::PerformanceOverlayTool(
        const Window& window,
        const Backend& backend,
        const Device& device,
        const ShaderManager& shaderManager,
        const RenderViewportTool& renderViewportTool
    )
        : ImGuiTool{ "PerformanceOverlay" }
        , m_Window{ window }
        , m_Backend{ backend }
        , m_Device{ device }
        , m_ShaderManager{ shaderManager }
        , m_RenderViewportTool{ renderViewportTool }
    {}

    void PerformanceOverlayTool::SetFrameRateStats(float frameRate, float dt)
    {
        m_FrameRate = frameRate;
        m_FrameDeltaTimeMs = dt;
    }

    void PerformanceOverlayTool::DrawWindow()
    {
        static constexpr auto backgroundColors = std::to_array(
        {
            IM_COL32(200, 50, 0, 255),
            IM_COL32(184, 100, 0, 255),
        });
        
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

            const ImVec2 windowPosition
            {
                IsEvenQuickly(+m_Location) ? workPosition.x + padding : workPosition.x + workSize.x - padding,
                IsDividedBy2Quickly(+m_Location) ? workPosition.y + padding : workPosition.y + workSize.y - padding,
            };

            const ImVec2 windowPositionPivot
            {
                IsEvenQuickly(+m_Location) ? 0.0f : 1.0f,
                IsDividedBy2Quickly(+m_Location) ? 0.0f : 1.0f,
            };

            ImGui::SetNextWindowPos(windowPosition, ImGuiCond_Always, windowPositionPivot);
            ImGui::SetNextWindowViewport(viewport->ID);

            windowFlags |= ImGuiWindowFlags_NoMove;
        }

        ImGui::SetNextWindowBgAlpha(0.7f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, backgroundColors[m_ShaderManager.IsEachShaderGood()]);
        ImGuiTool::DrawWindow(windowFlags);
        ImGui::PopStyleColor();
    }

    void PerformanceOverlayTool::DrawWindowContent()
    {
        const AdapterMemoryInfo adapterMemoryInfo = m_Backend.GetMainAdapterMemoryInfo();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0.0f, 0.0f });

        ImGui::Text(BenzinFormatData("Window: {} x {}", m_Window.GetWidth(), m_Window.GetHeight()));
        ImGui::Text(BenzinFormatData("Viewport: {} x {} {}", m_RenderViewportTool.GetWidth(), m_RenderViewportTool.GetHeight(), m_RenderViewportTool.IsValidForRendering() ? '+' : '-'));
        ImGui::Text(BenzinFormatData("{}", m_Backend.GetMainAdapterInfo().Name));
        ImGui::Text(BenzinFormatData("Fps: {:.1f} ({:.3f} ms)", m_FrameRate, m_FrameDeltaTimeMs));
        ImGui::Text(BenzinFormatData("Cpu: {}, Gpu: {}, Frame: {}", m_Device.GetCpuFrameIndex(), m_Device.GetCompletedGpuFrameIndex(), m_Device.GetActiveFrameIndex()));
        ImGui::Text(BenzinFormatData("FrameDelay: {}", m_Device.GetCpuFrameIndex() - m_Device.GetCompletedGpuFrameIndex()));
        ImGui::Text(BenzinFormatData("Local VRAM: {:.0f} / {:.0f} mb", adapterMemoryInfo.ProcessUsedVram.GetMb(), adapterMemoryInfo.VramOsBudget.GetMb()));
        ImGui::Text(BenzinFormatData("Host RAM: {:.0f} / {:.0f} mb", adapterMemoryInfo.ProcessUsedSharedRam.GetMb(), adapterMemoryInfo.SharedRamOsBudget.GetMb()));

        if (CommandLineArgs::GetBool("IsGpuValidationEnabled"))
        {
            ImGui::Text("!!! GPU Validation ENABLED");
        }

        if (CommandLineArgs::GetBool("IsSynchronizedCommandQueueValidationEnabled"))
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
