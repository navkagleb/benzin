#pragma once

#include "benzin/graphics2/imgui_pass.hpp"

namespace benzin
{

    class RenderViewportTool;
    class FlyCameraController;

    class FlyCameraTool : public ImGuiTool
    {
    public:
        explicit FlyCameraTool(RenderViewportTool& renderViewportTool);

    private:
        void DrawWindowContent() override;

        void DrawControllerProperties();
        void DrawViewProperties();
        void DrawProjectionProperties();

    private:
        FlyCameraController& m_Controller;
    };

}
