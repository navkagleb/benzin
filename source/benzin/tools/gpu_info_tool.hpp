#pragma once

#include "benzin/graphics2/imgui_pass.hpp"

namespace benzin
{

    class Backend;

    class GpuInfoTool : public ImGuiTool
    {
    public:
        explicit GpuInfoTool(const Backend& backend);

    private:
        void DrawWindowContent() override;

    private:
        const Backend& m_Backend;
    };

}
