#pragma once

#include <benzin/graphics2/imgui_pass.hpp>

namespace benzin
{

    class TickTimer;

}

namespace sandbox
{

    class TickTimerTool : public benzin::ImGuiTool
    {
    public:
        explicit TickTimerTool(const benzin::TickTimer& tickTimer);

    private:
        void SpawnImGui() override;

    private:
        const benzin::TickTimer& m_TickTimer;
    };

}
