#pragma once

#include <benzin/graphics2/imgui_pass.hpp>

namespace benzin
{

    class RenderResources;
    class RenderViewport;

    class RenderViewportTool : public ImGuiTool
    {
    public:
        RenderViewportTool(RenderViewport& viewport, RenderResources& resources);

        void DrawWindow() override;
        void DrawWindowContent() override;
        void PostDrawWindow() override;

    private:
        bool UpdateViewportSize();

    private:
        RenderViewport& m_Viewport;
        RenderResources& m_Resources;
    };

}
