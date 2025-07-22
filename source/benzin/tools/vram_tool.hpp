#pragma once

#include <benzin/graphics2/imgui_pass.hpp>

namespace benzin
{

    class VramTool : public ImGuiTool
    {
    public:
        VramTool(Device& device);

    private:
        void DrawWindowContent() override;

    private:
        Device& m_Device;
    };

}
