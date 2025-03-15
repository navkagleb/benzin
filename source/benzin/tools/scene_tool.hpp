#pragma once

#include "benzin/graphics2/imgui_pass.hpp"

namespace benzin
{

    class Scene;

    class SceneTool : public ImGuiTool
    {
    public:
        explicit SceneTool(Scene& scene);

    private:
        void DrawWindowContent() override;

        void DrawSun();
        void DrawSphericalLights();

    private:
        Scene& m_Scene;
    };

}
