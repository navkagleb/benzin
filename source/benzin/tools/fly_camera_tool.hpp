#pragma once

#include "benzin/engine/imgui_pass.hpp"

namespace benzin
{

    class FlyCameraController;

    class FlyCameraTool : public ImGuiTool
    {
    public:
        explicit FlyCameraTool(FlyCameraController& controller);

    private:
        void OnImGuiRender() override;

        void RenderImGuiControllerProperties();
        void RenderImGuiViewProperties();
        void RenderImGuiProjectionProperties();

    private:
        FlyCameraController& m_Controller;
    };

}
