#include "sandbox/bootstrap.hpp"
#include "sandbox/tools/tick_timer_tool.hpp"

#include <benzin/core/tick_timer.hpp>
#include <benzin/utility/time_utils.hpp>

namespace sandbox
{

    TickTimerTool::TickTimerTool(const benzin::TickTimer& tickTimer)
        : ImGuiTool{ "TickTimerTool", false }
        , m_TickTimer{ tickTimer }
    {}

    void TickTimerTool::SpawnImGui()
    {
        SpawnImGuiWindow([this]
        {
            ImGui::Text(BenzinFormatData(
                "DeltaTime: {:11.3f} ms, {:8.3f} s",
                benzin::ToFloatMs(m_TickTimer.GetDeltaTime()),
                benzin::ToFloatSec(m_TickTimer.GetDeltaTime())
            ));

            ImGui::Text(BenzinFormatData(
                "ElapsedTime: {:8.3f} ms, {:8.3f} s",
                m_TickTimer.GetElapsedTimeInMs(),
                benzin::MsToSec(m_TickTimer.GetElapsedTimeInMs())
            ));
        });
    }

}
