#pragma once

#include <benzin/graphics2/imgui_pass.hpp>

namespace benzin
{

    class FlyCameraController;

    class FlyCameraTool : public ImGuiTool
    {
    public:
        FlyCameraTool(FlyCameraController& controller);

        void DrawWindowContent() override;

    private:
        void DrawControllerProperties();
        void DrawViewProperties();
        void DrawProjectionProperties();

        FlyCameraController& m_Controller;
    };

}
